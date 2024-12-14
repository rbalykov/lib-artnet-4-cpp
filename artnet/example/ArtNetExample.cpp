#include "ArtNetController.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

 void myDataCallback(uint16_t universe, const uint8_t* data, uint16_t length);


void myDataCallback(uint16_t universe, const uint8_t* data, uint16_t length) {
    std::cout << "Received DMX data on universe: " << universe << ", length: " << length << ", data: ";
    for (int i = 0; i < length; ++i) {
        std::cout << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::endl;
}


int main() {
    ArtNet::ArtNetController controller;
    
    // Configure the controller
    if (!controller.configure("0.0.0.0", ArtNet::ARTNET_PORT, 0, 0, 0, "192.168.1.255")){
        std::cerr << "ArtNet: Configuration error" << std::endl;
        return 1;
    }
    

    if (!controller.start()) {
        std::cerr << "ArtNet: Start error" << std::endl;
        return 1;
    }
    
    // Set a callback
    controller.registerDataCallback(myDataCallback);
    
    // Send DMX data
    std::vector<uint8_t> dmxData(512);
    for(size_t i = 0; i < 512; i++){
        dmxData[i] = static_cast<uint8_t>(i);
    }
    
    controller.setDmxData(0, dmxData);
    
    while(controller.isRunning()){
        
        if(!controller.sendDmx())
            std::cout << "ArtNet: send error" << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }


    return 0;
}