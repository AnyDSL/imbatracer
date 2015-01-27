#ifndef IMBA_PROXY_HPP
#define IMBA_PROXY_HPP

namespace imba {

class Scene;

template <typename T>
struct SceneAccess {};

template <typename T>
class ReadWriteProxy {
    friend class Scene;

private:
    ReadWriteProxy(Scene* scene, int id)
        : scene_(scene), id_(id)
    {}

public:
    ~ReadWriteProxy() {
        SceneAccess<T>::notify_change(scene_, id_);
    }

    /// Safely use the object
    const T* operator -> () const {
        return SceneAccess<T>::read_only(scene_, id_);
    }

    /// Safely use the object
    T* operator -> () {
        return SceneAccess<T>::read_write(scene_, id_);
    }

    const T* get() const {
        return SceneAccess<T>::read_only(scene_, id_);
    }

private:
    Scene* scene_;
    int id_;
};

template <typename T>
class ReadOnlyProxy {
    friend class Scene;

private:
    ReadOnlyProxy(const Scene* scene, int id)
        : scene_(scene), id_(id)
    {}

public:
    /// Safely use the object
    const T* operator -> () const {
        return SceneAccess<T>::read_only(scene_, id_);
    }

    const T* get() const {
        return SceneAccess<T>::read_only(scene_, id_);
    }

private:
    const Scene* scene_;
    int id_;
};

} // namespace imba

#endif // IMBA_PROXY_HPP

