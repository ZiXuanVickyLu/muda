#include <muda/buffer/buffer_launch.h>

namespace muda
{
template <typename T>
void VarView<T>::copy_from(const T* val)
{
    BufferLaunch()
        .copy(*this, val)  //
        .wait();
}

template <typename T>
void VarView<T>::copy_to(T* val) const
{
    BufferLaunch()
        .copy(val, *this)  //
        .wait();
}

template <typename T>
void VarView<T>::copy_from(const VarView<T>& val)
{
    BufferLaunch()
        .copy(*this, val)  //
        .wait();
}

template <typename T>
Dense<T> VarView<T>::viewer() MUDA_NOEXCEPT
{
    return Dense<T>{m_data};
}

template <typename T>
CDense<T> VarView<T>::cviewer() const MUDA_NOEXCEPT
{
    return CDense<T>{m_data};
}
}  // namespace muda