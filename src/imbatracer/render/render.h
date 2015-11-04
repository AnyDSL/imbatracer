#ifndef IMBA_RENDER_H
#define IMBA_RENDER_H

#include "image.h"
#include "camera.h"
#include "integrator.h"

namespace imba {

template <typename StateType>
class Renderer {
public:
    Renderer(ThorinArray<::Node>& nodes, ThorinArray<::Vec4>& tris, Integrator<StateType>& s, int width, int height)
        : shader_(s), tex_(width, height)
    {
        target_ray_count_ = 1000000;
        int queue_capacity = target_ray_count_ * 3;
        
        // Upload nodes and tris.
        nodes.upload();
        tris.upload();
        
        // we need 3 queues in order to do traversal and shading in parallel
        queues_[0].resize(queue_capacity, &nodes, &tris);
        queues_[1].resize(queue_capacity, &nodes, &tris);
        queues_[2].resize(queue_capacity, &nodes, &tris);
        
        for (int pass = 0; pass < shader_.num_passes(); ++pass)
            shader_.get_ray_gen(pass).set_target_count(target_ray_count_);
    }

    // renders the scene
    Image& operator() (int n_samples) {
        // if there are less than this number of rays, the remaining rays are discarded
        constexpr int min_rays = 1000;
        
        clear_texture();
        
        for (int pass = 0; pass < shader_.num_passes(); ++pass) {
            shader_.get_ray_gen(pass).start_frame();
            
            // generate and traverse the first set of rays
            int cur_q = 0; // next queue used as input for the shader
            shader_.get_ray_gen(pass).fill_queue(queues_[cur_q]);
            queues_[cur_q].traverse();
            
            bool keep_rendering = true;  
            while (keep_rendering) {
                // prepare a second queue for traversal
                // if there are not enough rays in the queue yet, fill up with new samples from the camera
                int traversal_q = (cur_q + 1) % 3; 
                shader_.get_ray_gen(pass).fill_queue(queues_[traversal_q]);
                
                // if there are not enough rays left, do not traverse but still shade the resulting rays from the last traversal
                keep_rendering = queues_[traversal_q].size() > min_rays;
                
#pragma omp parallel sections
                {
                       
#pragma omp section
                    {
                        // traverse the queue if there are enough rays in it
                        if (keep_rendering) {
                            auto& q = queues_[traversal_q];
                            q.traverse();
                        }
                    }
                
#pragma omp section
                    {
                        // shade the first queue
                        int shader_out_q = (cur_q + 2) % 3;                
                        shader_.shade(pass, queues_[cur_q], tex_, queues_[shader_out_q]);
                        queues_[cur_q].clear();
                    }
                }
                
                // rotate the queues
                cur_q = traversal_q;
            }
            
            // remove leftover rays from all queues (so the next frame starts fresh)
            for (int i = 0; i < 3; ++i)
                queues_[i].clear();
        }
        
        return tex_;
    }
    
private:
    void clear_texture() {
        const int size = tex_.width() * tex_.height() * 4;
        for (int i = 0; i < size; i++) {
            tex_.pixels()[i] = 0.0f;
        }
    }

    Integrator<StateType>& shader_;    
    Image tex_;
    
    Hit* hits_[2];
    RayQueue<StateType> queues_[3];
    int target_ray_count_;
    
    int state_len_;
    unsigned char* shader_mem_;
};

} // namespace imba

#endif
