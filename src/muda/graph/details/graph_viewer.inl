namespace muda
{
MUDA_INLINE MUDA_GENERIC GraphViewer::GraphViewer(cudaGraphExec_t graph)
    : m_graph(graph)
{
}
MUDA_INLINE MUDA_GENERIC void GraphViewer::launch(cudaStream_t stream) const
{
#ifdef __CUDA_ARCH__
    MUDA_KERNEL_ASSERT(stream == cudaStreamGraphTailLaunch || stream == cudaStreamGraphFireAndForget,
                       "Launch Graph on device with invalid stream! "
                       "Only Stream::GraphTailLaunch{} and Stream::GraphFireAndForget{} are allowed");
#endif
    auto graph_viewer_error_code = cudaGraphLaunch(m_graph, stream);
    if(graph_viewer_error_code != cudaSuccess)
    {
        auto error_string = cudaGetErrorString(graph_viewer_error_code);
        MUDA_KERNEL_ERROR_WITH_LOCATION("GraphViewer[%s:%s]: launch error: %s(%d), GraphExec=%p",
                                        kernel_name(),
                                        name(),
                                        error_string,
                                        (int)graph_viewer_error_code,
                                        m_graph);
    }
}

MUDA_INLINE MUDA_DEVICE void muda::GraphViewer::tail_launch() const
{
#ifdef __CUDA_ARCH__
    launch(cudaStreamGraphTailLaunch);
#endif
}

MUDA_INLINE MUDA_DEVICE void GraphViewer::fire_and_forget() const
{
#ifdef __CUDA_ARCH__
    launch(cudaStreamGraphFireAndForget);
#endif
}
}  // namespace muda
