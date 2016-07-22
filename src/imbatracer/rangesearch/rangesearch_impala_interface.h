/* rangesearch_impala_interface.h : Impala interface file generated by impala */
#ifndef RANGESEARCH_IMPALA_INTERFACE_H
#define RANGESEARCH_IMPALA_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

struct RawDataInfo {
    float* begin;
    int stride;
};

struct Float3 {
    float x;
    float y;
    float z;
};

struct Buffer {
    int device;
    char* data;
};

struct PhotonHashGrid {
    float radius;
    float radius_sqr;
    float cell_size;
    float inv_cell_size;
    int photons_size;
    int indices_size;
    int cell_ends_size;
    struct Float3 bbox_min;
    struct Float3 bbox_max;
    int* indices;
    int* cell_ends;
    float* photons;
    struct Buffer photons_buf;
    struct Buffer indices_buf;
    struct Buffer cell_ends_buf;
    struct Buffer result_buf;
};

struct QueryResult {
    int size;
    int* data;
};

struct BatchQueryResult {
    int size;
    int* indices;
    int* offsets;
    struct Buffer indices_buf;
    struct Buffer offsets_buf;
};

struct PhotonHashGrid* build_hashgrid(struct RawDataInfo* info, int photon_cnt, int cell_size, float rad);
struct BatchQueryResult* batch_query_hashgrid(struct PhotonHashGrid* hg, float* query_poses, int size);
struct BatchQueryResult* batch_query_hashgrid2(struct PhotonHashGrid* hg, float* query_poses, int size);
void destroy_hashgrid(struct PhotonHashGrid* hg);
void release_query(struct QueryResult* arr);
void release_batch_query(struct BatchQueryResult* query);

#ifdef __cplusplus
}
#endif

#endif /* RANGESEARCH_IMPALA_INTERFACE_H */

