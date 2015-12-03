#ifndef IMBA_MASK_H
#define IMBA_MASK_H

namespace imba {

/// A container for opacity masks.
class MaskBuffer {
public:
    struct MaskDesc {
        int width;
        int height;
        int offset;

        MaskDesc()
            : width(1), height(1), offset(0)
        {}
        MaskDesc(int w, int h, int off)
            : width(w), height(h), offset(off)
        {}
    };

    MaskBuffer() {
        int align = 4;
        for (int i = 0; i < align; i++)
            buffer_.push_back(1);
    }

    /// Adds an image to the mask.
    MaskDesc append_mask(const Image& image) {
        const int offset = buffer_.size();
        descs_.emplace_back(image.width(), image.height(), offset);
        buffer_.resize(buffer_.size() + image.width() * image.height());
        for (int y = 0; y < image.height(); y++) {
            for (int x = 0; x < image.width(); x++) {
                auto pix = image(x, y);
                buffer_[offset + y * image.width() + x] = (pix.x + pix.y + pix.z > 0);
            }
        }
        return descs_.back();
    }

    /// Adds an existing image to the mask.
    void add_desc(const MaskDesc& desc = MaskDesc()) {
        descs_.emplace_back(desc);
    }

    const uint8_t* buffer() const { return buffer_.data(); }
    uint8_t* buffer() { return buffer_.data(); }

    int buffer_size() const { return buffer_.size(); }

    const MaskDesc* descs() const { return descs_.data(); }
    MaskDesc* descs() { return descs_.data(); }

    int mask_count() const { return descs_.size(); }

private:
    std::vector<uint8_t> buffer_;
    std::vector<MaskDesc> descs_;
};

} // namespace imba

#endif // IMBA_MASK_H
