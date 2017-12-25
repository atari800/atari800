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

    @IBOutlet weak var metalView: MTKView?
    
    let width = 384
    let height = 240
    
    override func viewDidLoad() {
        
        super.viewDidLoad()

        if let metalView = self.metalView  {
            
            metalView.device = MTLCreateSystemDefaultDevice()
            metalView.preferredFramesPerSecond = 60
            
            if metalView.device == nil {
                
                print("Metal is not supported on this device.");
                return
            }
            
            if let emulator = Atari800Emulator.shared() {
                
                if let renderer = emulator.renderer {
                    
                    renderer.setup(forView: metalView, widthInPixels: width, heightInScanLines: height);
                }
                self.metalView?.delegate?.mtkView(metalView, drawableSizeWillChange: metalView.drawableSize)
                
                let driver = Atari800AudioDriver()
                
                emulator.audioDriver = driver;
            }
        }
    }

    override func viewWillAppear() {
        
        if let emulator = Atari800Emulator.shared() {
            
            if let windowController = self.view.window?.windowController as? Atari800WindowController {
                
                emulator.keyboardHandler = windowController.newKeyboardHandler()
            }
            
            emulator.startEmulation()
        }
        
        super.viewWillAppear()
    }
    
    override func viewWillDisappear() {
        
        super.viewWillDisappear()
    }
    
    static var o: Int = 0;
    
    override var representedObject: Any? {
        didSet {
        // Update the view, if already loaded.
        }
    }
}

