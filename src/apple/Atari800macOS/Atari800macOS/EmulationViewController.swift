//
//  EmulationViewController.swift
//  Atari800Mac
//
//  Created by Rod Münch on 19/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

import Cocoa
import MetalKit
import Atari800EmulationCore_macOS

class EmulationViewController: NSViewController {

    let width = 384
    let height = 240
    
    weak var mtkView: MTKView?
    
    override func viewDidLoad() {
        super.viewDidLoad()

        // Do any additional setup after loading the view.
        if let mtkView = self.view as? MTKView {
            
            mtkView.device = MTLCreateSystemDefaultDevice()
            mtkView.preferredFramesPerSecond = 60
            
            if mtkView.device == nil {
                
                print("Metal is not supported on this device.");
                return
            }
            
            self.mtkView = mtkView
            
            if let emulator = Atari800Emulator.shared() {
                
                if let renderer = emulator.renderer {
                    
                    renderer.setup(forView: mtkView, widthInPixels: width, heightInScanLines: height);
                }
                mtkView.delegate?.mtkView(mtkView, drawableSizeWillChange: mtkView.drawableSize)
                
                let driver = Atari800AudioDriver()
                
                emulator.audioDriver = driver;
                
                emulator.startEmulation()
            }
        }
    }

    static var o: Int = 0;
    
    override var representedObject: Any? {
        didSet {
        // Update the view, if already loaded.
        }
    }
}

