/*
 * Copyright (C) 2018  Christian Berger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include <X11/Xlib.h>
#include <libyuv.h>

#include "cluon-complete.hpp"

#include "pixelink/camera.h"
#include "pixelink/pixelFormat.h"

int32_t main(int32_t argc, char **argv) {
    int32_t retCode{0};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( (0 == commandlineArguments.count("width")) ||
         (0 == commandlineArguments.count("height")) ||
         (0 == commandlineArguments.count("freq")) ) {
        std::cerr << argv[0] << " interfaces with the given IDS uEye camera (e.g., UI122xLE-M) and provides the captured image in two shared memory areas: one in I420 format and one in ARGB format." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --width=<width> --height=<height> [--pixel_clock=<value>] [--name.i420=<unique name for the shared memory in I420 format>] [--name.argb=<unique name for the shared memory in ARGB format>] [--verbose]" << std::endl;
        std::cerr << "         --name.i420:   name of the shared memory for the I420 formatted image; when omitted, 'ueye.i420' is chosen" << std::endl;
        std::cerr << "         --name.argb:   name of the shared memory for the I420 formatted image; when omitted, 'ueye.argb' is chosen" << std::endl;
        std::cerr << "         --pixel_clock: desired pixel clock (default: 10)" << std::endl;
        std::cerr << "         --width:       desired width of a frame" << std::endl;
        std::cerr << "         --height:      desired height of a frame" << std::endl;
        std::cerr << "         --freq:        desired frequency" << std::endl;
        std::cerr << "         --verbose:     display captured image" << std::endl;
        std::cerr << "Example: " << argv[0] << " --width=752 --height=480 --pixel_clock=10 --freq=20 --verbose" << std::endl;
        retCode = 1;
    }
    else {
//        const uint32_t WIDTH{static_cast<uint32_t>(std::stoi(commandlineArguments["width"]))};
//        const uint32_t HEIGHT{static_cast<uint32_t>(std::stoi(commandlineArguments["height"]))};
        const bool VERBOSE{commandlineArguments.count("verbose") != 0};
        const uint32_t PIXEL_CLOCK{(commandlineArguments["pixel_clock"].size() != 0) ?static_cast<uint32_t>(std::stoi(commandlineArguments["pixel_clock"])) : 10};

        const float FREQ{static_cast<float>(std::stof(commandlineArguments["freq"]))};
        if ( !(FREQ > 0) ) {
            std::cerr << "[opendlv-device-camera-ueye]: freq must be larger than 0; found " << FREQ << "." << std::endl;
            return retCode = 1;
        }

        // Set up the names for the shared memory areas.
        std::string NAME_I420{"ueye.i420"};
        if ((commandlineArguments["name.i420"].size() != 0)) {
            NAME_I420 = commandlineArguments["name.i420"];
        }
        std::string NAME_ARGB{"ueye.argb"};
        if ((commandlineArguments["name.argb"].size() != 0)) {
            NAME_ARGB = commandlineArguments["name.argb"];
        }

        // Initialize camera.
        PxLCamera pxLCamera(0);
        PXL_RETURN_CODE rc;
        PXL_ROI roi;
        rc = pxLCamera.getRoiValue(&roi);
        if (!API_SUCCESS(rc)) {
          std::cerr << "error!";
        }
        const uint32_t WIDTH{static_cast<uint32_t>(roi.m_width)};
        const uint32_t HEIGHT{static_cast<uint32_t>(roi.m_height)};

        float currentValue;
        pxLCamera.getValue(FEATURE_PIXEL_FORMAT, &currentValue);
        std::cout << "Current pixel format: " << currentValue << std::endl; // PIXEL_FORMAT_BAYER8_RGGB      7

        auto image_size = pxLCamera.getImageSize();
        std::unique_ptr<uint8_t []> buffer = std::make_unique<uint8_t []>(image_size);
        // FIXME easy 20180331 - x265 ffmpeg only works at 1920x1080. Need to manually set ROI first.

        rc = pxLCamera.play();


        // Initialize shared memory.
        std::unique_ptr<cluon::SharedMemory> sharedMemoryI420(new cluon::SharedMemory{NAME_I420, WIDTH * HEIGHT * 3/2});
        if (!sharedMemoryI420 || !sharedMemoryI420->valid()) {
            std::cerr << "[opendlv-device-camera-ueye]: Failed to create shared memory '" << NAME_I420 << "'." << std::endl;
            return retCode = 1;
        }

        std::unique_ptr<cluon::SharedMemory> sharedMemoryARGB(new cluon::SharedMemory{NAME_ARGB, WIDTH * HEIGHT * 4});
        if (!sharedMemoryARGB || !sharedMemoryARGB->valid()) {
            std::cerr << "[opendlv-device-camera-ueye]: Failed to create shared memory '" << NAME_ARGB << "'." << std::endl;
            return retCode = 1;
        }

        if ( (sharedMemoryI420 && sharedMemoryI420->valid()) &&
             (sharedMemoryARGB && sharedMemoryARGB->valid()) ) {
            std::clog << "[opendlv-device-camera-ueye]: Data from uEye camera available in I420 format in shared memory '" << sharedMemoryI420->name() << "' (" << sharedMemoryI420->size() << ") and in ARGB format in shared memory '" << sharedMemoryARGB->name() << "' (" << sharedMemoryARGB->size() << ")." << std::endl;

            // Accessing the low-level X11 data display.
            Display* display{nullptr};
            Visual* visual{nullptr};
            Window window{0};
            XImage* ximage{nullptr};
            if (VERBOSE) {
                display = XOpenDisplay(NULL);
                visual = DefaultVisual(display, 0);
                window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, WIDTH, HEIGHT, 1, 0, 0);
                sharedMemoryARGB->lock();
                {
                    ximage = XCreateImage(display, visual, 24, ZPixmap, 0, sharedMemoryARGB->data(), WIDTH, HEIGHT, 32, 0);
                }
                sharedMemoryARGB->unlock();
                XMapWindow(display, window);
            }

            while (!cluon::TerminateHandler::instance().isTerminated.load()) {
                    uint8_t *ueyeImagePtr{buffer.get()};
                    rc = pxLCamera.getNextFrame(image_size, buffer.get());
                    if (API_SUCCESS(rc)) {
                        cluon::data::TimeStamp ts{cluon::time::now()};
                        // Transform data as I420 in sharedMemoryI420.
                        sharedMemoryI420->lock();
                        sharedMemoryI420->setTimeStamp(ts);
                        {
                            libyuv::I422ToI420(reinterpret_cast<uint8_t*>(ueyeImagePtr), WIDTH,
                                               reinterpret_cast<uint8_t*>(ueyeImagePtr+(WIDTH * HEIGHT)), WIDTH/2,
                                               reinterpret_cast<uint8_t*>(ueyeImagePtr+(WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH/2,
                                               reinterpret_cast<uint8_t*>(sharedMemoryI420->data()), WIDTH,
                                               reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT)), WIDTH/2,
                                               reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH/2,
                                               WIDTH, HEIGHT);
                        }
                        sharedMemoryI420->unlock();

                        sharedMemoryARGB->lock();
                        sharedMemoryARGB->setTimeStamp(ts);
                        {
                            libyuv::I420ToARGB(reinterpret_cast<uint8_t*>(sharedMemoryI420->data()), WIDTH,
                                               reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT)), WIDTH/2,
                                               reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH/2,
                                               reinterpret_cast<uint8_t*>(sharedMemoryARGB->data()), WIDTH * 4, WIDTH, HEIGHT);

                            if (VERBOSE) {
                                XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
                            }
                        }
                        sharedMemoryARGB->unlock();

                        sharedMemoryI420->notifyAll();
                        sharedMemoryARGB->notifyAll();
                }
            }

            if (VERBOSE) {
                XCloseDisplay(display);
            }
        }

        // Free camera.
        if (pxLCamera.streaming())
          pxLCamera.stop();
    }
    return retCode;
}

