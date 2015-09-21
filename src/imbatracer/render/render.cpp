#include "render.h"
#include "../core/float4.h"
#include "thorin_runtime.h"

#include <cassert>
#include <thread>

//#define RENDER_STATS
#define PARALLEL_TRAVERSAL

#ifdef RENDER_STATS
#include <chrono>
#include <iostream>
#endif

imba::Render::Render(Camera& c, ThorinVector<Node>& nodes, ThorinVector<Vec4>& tris, Shader& s, int width, int height)
    : ray_gen_(c), nodes_(nodes), tris_(tris), shader_(s), tex_(width, height), ray_count_(width * height * 24)
{            
    // allocate memory for intersection data, written by traversal
    hits_[0] = thorin_new<Hit>(ray_count_);
    hits_[1] = thorin_new<Hit>(ray_count_);
    
    // we need 3 queues in order to do traversal and shading in parallel
    queues_[0].resize(ray_count_, s.state_len(), s.initial_state());
    queues_[1].resize(ray_count_, s.state_len(), s.initial_state());
    queues_[2].resize(ray_count_, s.state_len(), s.initial_state());
}

imba::Image& imba::Render::operator() (int n_samples) {
    assert(n_samples >= 2 && "number of samples must be at least 2");
    const int min_rays = tex_.width() * tex_.height() * 5;
    const int concurrent_samples = 8;
    n_samples /= concurrent_samples;
    
    clear_buffer();
    
#ifdef RENDER_STATS
    std::cout << "Start rendering:" << std::endl;
    std::cout << "samples: " << n_samples * concurrent_samples << std::endl;
    std::cout << "concurrent samples: " << concurrent_samples << std::endl;
    std::cout << "min ray count: " << min_rays << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    int total_shader_calls = 0;
    int total_traversal_time = 0;
    int total_shader_time = 0;
    int traversal_calls = 0;
    long total_traversal_rays = 0;
#endif
    
    // generate and traverse the first set of rays
    int cur_q = 0; // next queue used as input for the shader
    int cur_hit = 0;
    ray_gen_(queues_[cur_q], rng_, concurrent_samples);
    RayQueue::Entry ray_data = queues_[cur_q].peek();
    
#ifdef RENDER_STATS
    auto begin = std::chrono::high_resolution_clock::now();
#endif

    traverse_accel(nodes_.data(), ray_data.rays, tris_.data(), hits_[cur_hit], ray_data.ray_count);
    
#ifdef RENDER_STATS
    auto end = std::chrono::high_resolution_clock::now();
    total_traversal_time += std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    traversal_calls++;
    total_traversal_rays += ray_data.ray_count;
#endif
    
    int created_samples = 1;  
    bool keep_rendering = true;  
    while (keep_rendering) {
        // prepare a second queue for traversal
        // if there are not enough rays in the queue yet, fill up with new samples from the camera
        int traversal_q = (cur_q + 1) % 3; 
        if (queues_[traversal_q].size() < min_rays && created_samples <= n_samples) {
            ray_gen_(queues_[traversal_q], rng_, concurrent_samples);
            created_samples++;
        }
        
        keep_rendering = (created_samples <= n_samples);// || (queues_[traversal_q].size() > 1000000);
        
        // launch a thread to traverse the second queue
        std::thread traversal;
        if (keep_rendering) {
#ifdef RENDER_STATS
            traversal = std::thread([&total_traversal_time, &traversal_calls, &total_traversal_rays, this, traversal_q, cur_hit] () {
#else
            traversal = std::thread([this, traversal_q, cur_hit] () {
#endif
                RayQueue::Entry ray_data = queues_[traversal_q].peek();
                
#ifdef RENDER_STATS
                auto begin = std::chrono::high_resolution_clock::now();
#endif      

                traverse_accel(nodes_.data(), ray_data.rays, tris_.data(), hits_[!cur_hit], ray_data.ray_count);
                
#ifdef RENDER_STATS
                auto end = std::chrono::high_resolution_clock::now();
                total_traversal_time += std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
                traversal_calls++;
                total_traversal_rays += ray_data.ray_count;
#endif
            });
            
#ifndef PARALLEL_TRAVERSAL
            traversal.join();
#endif
        }
        
#ifdef RENDER_STATS
        auto time_start = std::chrono::high_resolution_clock::now();
#endif
        // shade the first queue
        int shader_out_q = (cur_q + 2) % 3;
        RayQueue::Entry ray_data = queues_[cur_q].pop();
        shader_(ray_data.rays, hits_[cur_hit], ray_data.state_data, ray_data.pixel_indices, ray_data.ray_count, tex_, queues_[shader_out_q], rng_);
        
#ifdef RENDER_STATS
        total_shader_calls++;
        auto time_end = std::chrono::high_resolution_clock::now();
        total_shader_time += std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count();
#endif
        
#ifdef PARALLEL_TRAVERSAL
        if (keep_rendering) {
            // wait for traversal to finish
            traversal.join();
        }
#endif
        
        // rotate the queues and swap the hit buffers
        cur_q = traversal_q;
        cur_hit = !cur_hit;
    }
    
#ifdef RENDER_STATS
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    std::cout << "Rendering finished after " << total_ms << " ms." << std::endl;
    std::cout << "Time spent on traversal: " << total_traversal_time << " ms (" << total_traversal_time / static_cast<double>(total_ms) * 100.0 << "%)" << std::endl;
    std::cout << "Time spent on shading: " << total_shader_time << " ms (" << total_shader_time / static_cast<double>(total_ms) * 100.0 << "%)" << std::endl;
    
    std::cout << "Shader called " << total_shader_calls << " times" << std::endl;
    std::cout << "Traversal called " << traversal_calls << " times" << std::endl;
    
    std::cout << "Average rays per call to traversal " << total_traversal_rays / static_cast<double>(traversal_calls) << std::endl;
    
    ray_data = queues_[(cur_q + 1) % 3].pop();
    std::cout << "Leftover rays: " << ray_data.ray_count << std::endl;
    
#ifdef PARALLEL_TRAVERSAL
    std::cout << "Traversal and shading done in parallel." << std::endl;
#endif
    std::cout << "=======================================" << std::endl << std::endl;
#endif
    
    // remove leftover rays from all queues (so the next frame starts fresh)
    for (int i = 0; i < 3; ++i)
        queues_[i].pop();
    
    return tex_;
}

void imba::Render::clear_buffer() {
    const int size = tex_.width() * tex_.height() * 4;
    for (int i = 0; i < size; i++) {
        tex_.pixels()[i] = 0.0f;
    }
}
