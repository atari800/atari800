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
    
    static let ShowCartridgeTypesSegue = NSStoryboardSegue.Identifier("Show Cartridge Types")
    
    var cartridgeSize: Int = 0
    var cartridgeFileName: String?
    var cartridgeCompletion: Atari800CartridgeSelectionHandler? = nil
    
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
            }
        }
    }

    override func viewWillAppear() {
        
        if let emulator = Atari800Emulator.shared() {
            
            if let windowController = self.view.window?.windowController as? Atari800WindowController {
                
                emulator.keyboardHandler = windowController.newKeyboardHandler()
                emulator.portHandler = windowController.newPortHandler()
            }
            
            emulator.startEmulation()
        }
        
        super.viewWillAppear()
    }
    
    override func viewWillDisappear() {
        
        super.viewWillDisappear()
    }
    
    override func prepare(for segue: NSStoryboardSegue, sender: Any?) {
    
        if segue.identifier == EmulationViewController.ShowCartridgeTypesSegue {
            
            if let cartridgeTypesController = segue.destinationController as? CartridgeTypesViewController {
                
                cartridgeTypesController.cartridgeFileName = self.cartridgeFileName
                cartridgeTypesController.cartridgeCompletion = self.cartridgeCompletion
                cartridgeTypesController.cartridgeSize = self.cartridgeSize
            }
        }
    }
    
    @IBAction func reset(_ sender: Any?) {
        
        Atari800Emulator.shared().reset()
    }
    
    @IBAction func openDocument(_ sender: Any?) {
    
        let panel = NSOpenPanel();
        panel.begin { (response: NSApplication.ModalResponse) in
            
            if response == NSApplication.ModalResponse.OK {
                
                if let url = panel.urls.first {
                    
                    if let emulator = Atari800Emulator.shared() {
                        
                        DispatchQueue.main.async {
                            
                            emulator.loadFile(url) { (ok, error) in
                                
                                NSDocumentController.shared.noteNewRecentDocumentURL(url)
                            }
                        }
                    }
                }
            }
        }
    }
}
