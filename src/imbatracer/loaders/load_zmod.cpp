#include <memory>
#include <fstream>
#include <zlib.h>

#include "load_zmod.h"

namespace imba {

template <typename T>
void read_compressed(std::istream& is, std::vector<T>& data, int count) {
    data.resize(count);

    uLongf buf_size;
    is.read((char*)&buf_size, sizeof(uLongf));
    std::unique_ptr<Bytef[]> buf(new Bytef[buf_size]);
    is.read((char*)buf.get(), buf_size);

    uLongf size = count * sizeof(T);
    uncompress((Bytef*)data.data(), &size, buf.get(), buf_size);
}

bool load_zmod(const Path& path, zmod::File& file) {
    std::ifstream in(path, std::ifstream::binary);
    if (!in) return false;

    char header[4];
    in.read(header, 4);
    if (header[0] != 'Z' || header[1] != 'M' ||
        header[2] != 'O' || header[3] != 'D') {
        return false;
    }

    int32_t vert_count, tri_count, mtl_count;

    in.read((char*)&tri_count, sizeof(int32_t));
    in.read((char*)&vert_count, sizeof(int32_t));
    in.read((char*)&mtl_count, sizeof(int32_t));

    read_compressed(in, file.indices, tri_count * 3);
    read_compressed(in, file.vertices, vert_count);
    read_compressed(in, file.normals, vert_count);
    read_compressed(in, file.texcoords, vert_count);
    read_compressed(in, file.mat_ids, tri_count);

    for (int i = 0; i < mtl_count; i++) {
        int32_t len;
        in.read((char*)&len, sizeof(int32_t));
        std::string str(len + 1, '\0');
        in.read(&str[0], len);
        file.mat_names.push_back(str);
    }

    return true;
}

}
