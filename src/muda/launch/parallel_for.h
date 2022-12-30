#pragma once
#include "launch_base.h"
#include <stdexcept>
#include <exception>

namespace muda
{
namespace details
{
    template <typename F>
    __global__ void parallelForKernel(F f, int begin, int end, int step)
    {
        auto tid = blockIdx.x * blockDim.x + threadIdx.x;
        auto i   = begin + step * tid;
        if(i < end)
            f(i);
    }

    /// <summary>
    ///
    /// </summary>
    template <typename F>
    __global__ void gridStrideLoopKernel(F f, int begin, int end, int step)
    {
        auto tid = blockIdx.x * blockDim.x + threadIdx.x;
        auto k   = tid;
        auto i   = begin + step * k;
        while(i < end)
        {
            f(i);
            // update k by grid stride
            k += blockDim.x * gridDim.x;
            i = begin + step * k;
        }
    }
}  // namespace details


/// <summary>
/// parallel_for
/// usage:
///		parallel_for(16)
///			.apply(16, [=] __device__(int i) mutable { printf("var=%d, i = %d\n");}, true);
/// </summary>
class parallel_for : public launch_base<parallel_for>
{
    int    gridDim;
    int    blockDim;
    size_t sharedMemSize;

  public:
    template <typename F>
    class kernelData
    {
      public:
        int begin;
        int step;
        int end;
        F   callable;
        template <typename U>
        kernelData(int begin, int step, int end, U&& callable)
            : begin(begin)
            , step(step)
            , end(end)
            , callable(std::forward<U>(callable))
        {
        }
    };

    /// <summary>
    /// calculate grid dim automatically to cover the range
    /// </summary>
    /// <param name="blockDim">block dim to use</param>
    /// <param name="sharedMemSize"></param>
    /// <param name="stream"></param>
    parallel_for(int blockDim, size_t sharedMemSize = 0, cudaStream_t stream = nullptr)
        : launch_base(stream)
        , gridDim(0)
        , blockDim(blockDim)
        , sharedMemSize(sharedMemSize)
    {
    }

    /// <summary>
    /// use Grid-Stride Loops to cover the range
    /// </summary>
    /// <param name="blockDim"></param>
    /// <param name="gridDim"></param>
    /// <param name="sharedMemSize"></param>
    /// <param name="stream"></param>
    parallel_for(int gridDim, int blockDim, size_t sharedMemSize = 0, cudaStream_t stream = nullptr)
        : launch_base(stream)
        , gridDim(gridDim)
        , blockDim(blockDim)
        , sharedMemSize(sharedMemSize)
    {
    }

    /// <summary>
    /// apply parallel for: [begin, end) by step
    /// </summary>
    /// <typeparam name="F">callable : void (int i)</typeparam>
    /// <param name="begin"></param>
    /// <param name="step"></param>
    /// <param name="end"></param>
    /// <param name="f"></param>
    /// <param name="wait"></param>
    /// <returns></returns>
    template <typename F>
    parallel_for& apply(int begin, int end, int step, F&& f)
    {
        using CallableType = raw_type_t<F>;
        static_assert(std::is_invocable_v<CallableType, int>, "f:void (int i)");

        checkInput(begin, end, step);

        if(gridDim <= 0)  // parallel_for
        {
            if(begin != end)
            {
                // calculate the blocks we need
                auto nBlocks = calculateGridDim(begin, end, step);
                details::parallelForKernel<<<nBlocks, blockDim, sharedMemSize, stream_>>>(
                    f, begin, end, step);
            }
        }
        else  // grid stride loop
        {
            details::gridStrideLoopKernel<<<gridDim, blockDim, sharedMemSize, stream_>>>(
                f, begin, end, step);
        }
        return *this;
    }

    /// <summary>
    /// apply parallel for: [begin, begin + count) by step 1
    /// </summary>
    /// <typeparam name="F">callable : void (int i)</typeparam>
    /// <param name="begin"></param>
    /// <param name="count"></param>
    /// <param name="f"></param>
    /// <param name="wait"></param>
    /// <returns></returns>
    template <typename F>
    parallel_for& apply(int begin, int count, F&& f)
    {
        return apply(begin, begin + count, 1, std::forward<F>(f));
    }

    /// <summary>
    /// apply parallel for: [0, count) by step 1
    /// </summary>
    /// <typeparam name="F">callable : void (int i)</typeparam>
    /// <param name="count"></param>
    /// <param name="f"></param>
    /// <param name="wait"></param>
    /// <returns></returns>
    template <typename F>
    parallel_for& apply(int count, F&& f)
    {
        return apply(0, count, 1, std::forward<F>(f));
    }

    /// <summary>
    /// create parallel for: [begin, end) by step, as graph kernel node parameters
    /// </summary>
    /// <typeparam name="F"></typeparam>
    /// <param name="parallelForKernel"></param>
    /// <param name="step"></param>
    /// <param name="end"></param>
    /// <param name="f"></param>
    /// <returns></returns>
    template <typename F>
    [[nodiscard]] auto asNodeParms(int begin, int end, int step, F&& f)
    {
        using CallableType = raw_type_t<F>;
        static_assert(std::is_invocable_v<CallableType, int>, "f:void (int i)");

        checkInput(begin, end, step);

        auto parms = std::make_shared<kernelNodeParms<kernelData<CallableType>>>(
            begin, step, end, std::forward<F>(f));
        if(gridDim <= 0)
        {
            auto nBlocks = calculateGridDim(begin, end, step);
            parms->func((void*)details::parallelForKernel<CallableType>);
            parms->gridDim(nBlocks);
        }
        else
        {
            parms->func((void*)details::gridStrideLoopKernel<CallableType>);
            parms->gridDim(gridDim);
        }

        parms->blockDim(blockDim);
        parms->sharedMemBytes(sharedMemSize);
        parms->parse(
            [](kernelData<CallableType>& p) -> std::vector<void*> {
                return {&p.callable, &p.begin, &p.end, &p.step};
            });
        return parms;
    }

    /// <summary>
    /// create parallel for: [begin, begin + count] by step 1, as graph kernel node parameters
    /// </summary>
    /// <param name="begin"></param>
    /// <param name="count"></param>
    /// <returns></returns>
    template <typename F>
    [[nodiscard]] auto asNodeParms(int begin, int count, F&& f)
    {
        return asNodeParms(begin, begin + count, 1, std::forward<F>(f));
    }


    /// <summary>
    /// create parallel for: [0, count] by step 1, as graph kernel node parameters
    /// </summary>
    /// <param name="count"></param>
    template <typename F>
    [[nodiscard]] auto asNodeParms(int count, F&& f)
    {
        return asNodeParms(0, count, 1, std::forward<F>(f));
    }

  private:
    int calculateGridDim(int begin, int end, int step)
    {
        auto stride     = end - begin;
        auto nMinthread = stride / step + ((stride % step) != 0 ? 1 : 0);
        auto nMinblocks = nMinthread / blockDim + ((nMinthread % blockDim) > 0 ? 1 : 0);
        return nMinblocks;
    }

    static void checkInput(int begin, int end, int step)
    {
        if(step == 0)
            throw std::logic_error("step should not be 0!");
        else if(step * (end - begin) < 0)
            throw std::logic_error("step direction is not consistent with [begin, end)!");
        //else if(begin == end)
        //    throw std::logic_error("begin should not equal to end!");
    }
};
}  // namespace muda