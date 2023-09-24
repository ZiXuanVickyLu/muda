#include <catch2/catch.hpp>
#include <muda/muda.h>
#include <muda/container.h>
#include <muda/compute_graph/compute_graph_builder.h>
#include <muda/compute_graph/compute_graph.h>

using namespace muda;
using Vector3 = Eigen::Vector3f;

void compute_graph_simple()
{
    // define graph vars
    ComputeGraph graph;

    auto& N   = graph.create_var<size_t>("N");
    auto& x_0 = graph.create_var<Dense1D<Vector3>>("x_0");
    auto& x   = graph.create_var<Dense1D<Vector3>>("x");
    auto& y   = graph.create_var<Dense1D<Vector3>>("y");


    graph.create_node("cal_x_0") << [&]
    {
        ParallelFor(256).apply(N.eval(),
                               [x_0 = x_0.eval()] __device__(int i) mutable
                               {
                                   // simple set
                                   x_0(i) = Vector3::Ones();
                               });
    };

    graph.create_node("copy_to_x") << [&]
    {
        Memory().transfer(x.eval().data(), x_0.ceval().data(), N * sizeof(Vector3));
    };

    graph.create_node("copy_to_y") << [&]
    {
        Memory().transfer(y.eval().data(), x_0.ceval().data(), N * sizeof(Vector3));
    };

    graph.create_node("print_x_y") << [&]
    {
        ParallelFor(256).apply(N.eval(),
                               [x = x.ceval(), y = y.ceval(), N = N.eval()] __device__(int i) mutable
                               {
                                   if(N <= 10)
                                       printf("[%d] x = (%f,%f,%f) y = (%f,%f,%f) \n",
                                              i,
                                              x(i).x(),
                                              x(i).y(),
                                              x(i).z(),
                                              y(i).x(),
                                              y(i).y(),
                                              y(i).z());
                               });
    };

    graph.graphviz(std::cout);

    auto N_value    = 10;
    auto x_0_buffer = DeviceVector<Vector3>(N_value);
    auto x_buffer   = DeviceVector<Vector3>(N_value);
    auto y_buffer   = DeviceVector<Vector3>(N_value);

    x_0.update(make_viewer(x_0_buffer));
    x.update(make_viewer(x_buffer));
    y.update(make_viewer(y_buffer));
    N.update(N_value);

    graph.launch();
    graph.launch(true);
    Launch::wait_device();

    // update: change N
    auto f = [&](int new_N)
    {
        N_value = new_N;
        x_0_buffer.resize(N_value);
        x_buffer.resize(N_value);
        y_buffer.resize(N_value);

        N.update(N_value);
        x_0.update(make_viewer(x_0_buffer));
        x.update(make_viewer(x_buffer));
        y.update(make_viewer(y_buffer));

        graph.update();

        auto t1 = profile_host(
            [&]
            {
                graph.launch();
                Launch::wait_device();
            });
        auto t2 = profile_host(
            [&]
            {
                graph.launch(true);
                Launch::wait_device();
            });

        std::cout << "N = " << N_value << std::endl;
        std::cout << "graph launch time: " << t1 << "ms" << std::endl;
        std::cout << "single stream launch time: " << t2 << "ms" << std::endl
                  << std::endl;
    };

    f(1M);
    f(10M);
    f(100M);
}

void compute_graph_test()
{
    // resources
    size_t                N = 10;
    DeviceVector<Vector3> x_0(N);
    DeviceVector<Vector3> x(N);
    DeviceVector<Vector3> v(N);
    DeviceVector<Vector3> dt(N);
    DeviceVar<float>      toi;
    HostVector<Vector3>   h_x(N);

    // define graph vars
    ComputeGraph graph;

    auto& var_x_0 = graph.create_var("x_0", make_viewer(x_0));
    auto& var_h_x = graph.create_var("h_x", make_viewer(h_x));
    auto& var_x   = graph.create_var("x", make_viewer(x));
    auto& var_v   = graph.create_var("v", make_viewer(v));
    auto& var_toi = graph.create_var("toi", make_viewer(toi));
    auto& var_dt  = graph.create_var("dt", 0.1);
    auto& var_N   = graph.create_var("N", N);

    // define graph nodes
    graph.create_node("cal_v") << [&]
    {
        ParallelFor(256)  //
            .apply(var_N,
                   [v = var_v.eval(), dt = var_dt.eval()] __device__(int i) mutable
                   {
                       // simple set
                       v(i) = Vector3::Ones() * dt * dt;
                   });
    };

    graph.create_node("cd") << [&]
    {
        ParallelFor(256)  //
            .apply(var_N,
                   [x   = var_x.ceval(),
                    v   = var_v.ceval(),
                    dt  = var_dt.eval(),
                    toi = var_toi.ceval()] __device__(int i) mutable
                   {
                       // collision detection
                   });
    };

    graph.create_node("cal_x") << [&]
    {
        ParallelFor(256).apply(var_N,
                               [x   = var_x.eval(),
                                v   = var_v.ceval(),
                                dt  = var_dt.eval(),
                                toi = var_toi.ceval()] __device__(int i) mutable
                               {
                                   // integrate
                                   x(i) += v(i) * toi * dt;
                               });
    };

    graph.create_node("transfer") << [&]
    {
        Memory().transfer(var_x_0.eval().data(), var_x.ceval().data(), var_N * sizeof(Vector3));
    };

    graph.create_node("download") << [&]
    {
        Memory().download(var_h_x.eval().data(), var_x.ceval().data(), var_N * sizeof(Vector3));
    };

    graph.graphviz(std::cout);
    //graph.launch(true);
    //graph.launch();
}

TEST_CASE("compute_graph_test", "[default]")
{
    compute_graph_simple();
    compute_graph_test();
}
