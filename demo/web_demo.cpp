//
//  web_demo.cpp
//
//  Created by MNN on 2023/03/25.
//  ZhaodeWang
//

#include "llm.hpp"
#include "httplib.h"
#include "CLI11.hpp"
#include <iostream>
#include <thread>
#ifdef WITH_CUDA
#include <cuda.h>
#include <cuda_runtime.h>
#endif // DEBUG

int main(int argc, const char* argv[]) {
    CLI::App app{"web demo for llm.cpp"};
#ifdef WITH_CUDA
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    std::cout << "Device name: " << prop.name << std::endl; 
    std::cout << "Compute capability: " << prop.major << "." << prop.minor << std::endl;
    // Query device for FP16 support
    int fp16_support;
    cudaDeviceGetAttribute(&fp16_support, cudaDevAttrComputeCapabilityMajor, 0);
    if (fp16_support >= 7) {
        std::cout << "GPU Device supports FP16 computation" << std::endl;
    } else {
        std::cout << "GPU Device does not support FP16 computation" << std::endl;
    }
    size_t free_byte;
    size_t total_byte;
    cudaMemGetInfo(&free_byte, &total_byte);
    double free_db = (double)free_byte;
    double total_db = (double)total_byte;
    double free_db_g = free_db / 1024.0 / 1024.0 / 1024.0;
    double total_db_g = total_db / 1024.0 / 1024.0 / 1024.0;
    std::cout << "Total global memory: " << total_db_g << "G" << std::endl;
    std::cout << "Total available memory: " << free_db_g << "G" << std::endl;
    float gpusize = free_db_g;
#else
    float gpusize = 8.0;
#endif
    float cpusize = 8;
    std::string model_dir = "../resource/models";
    std::string web_dir = "../resource/web";
    std::string data_type = "fp16";
    std::string tokenizer_dir = "../resource/tokenizer";
    app.add_option("-c,--cpusize", cpusize,"cpu memory size(G), default is 8G.");
    app.add_option("-g,--gpusize", gpusize,"gpu memory size(G)");
    app.add_option("-m,--model_dir", model_dir, "model directory");
    app.add_option("-w,--web_dir", web_dir, "web directory");

    CLI11_PARSE(app, argc, argv);

    std::cout << "model path is " << model_dir << std::endl;
    std::unique_ptr<Llm> llm(Llm::createLLM(model_dir));
    llm->load(model_dir);
    
    std::stringstream ss;
    httplib::Server svr;
    std::atomic_bool waiting;
    waiting = false;
    std::string last_request = "";
    auto chat = [&](std::string str) {
        waiting = true;
        llm->response(str, &ss);
        std::cout << "### response : " << ss.str() << std::endl;
        ss << "<eop>";
        waiting = false;
    };
    svr.Post("/chat", [&](const httplib::Request &req, httplib::Response &res) {
        if (req.body == last_request) {
            res.set_content(ss.str(), "text/plain");
            return;
        }
        if (waiting) {
            res.set_content(ss.str(), "text/plain");
        } else {
            ss.str("");
            std::cout << "### request : " << req.body << std::endl;
            last_request = req.body;
            std::thread chat_thread(chat, last_request);
            chat_thread.detach();
        }
    });
    svr.set_mount_point("/", web_dir);
    printf(">>> please open http://0.0.0.0:5088\n");
    fflush(stdout);
    svr.listen("0.0.0.0", 5088);
    printf(">>> end\n");
    return 0;
}
