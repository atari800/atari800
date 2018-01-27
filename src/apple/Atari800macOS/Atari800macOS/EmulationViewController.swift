//
//  EmulationViewController.swift
//  Atari800Mac
//
//  Created by Rod Münch on 19/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

import Cocoa
import MetalKit
import Atari800EmulationCore

struct CartridgeDetails {
    let fileName: String
    let types: [NSNumber: String]
    let completion: Atari800CartridgeSelectionHandler
    
    init(fileName: String, types: [NSNumber: String], completion: @escaping Atari800CartridgeSelectionHandler) {
        
        self.fileName = fileName;
        self.types = types
        self.completion = completion
    }
}

class EmulationViewController: NSViewController {

    @IBOutlet weak var metalView: MTKView?
    
    static let ShowCartridgeTypesSegue = NSStoryboardSegue.Identifier("Show Cartridge Types")
    static let ShowDrivesSegue = NSStoryboardSegue.Identifier("Show Drives")
    
    var cartridgeDetails: CartridgeDetails?
    
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
                
                cartridgeTypesController.cartridgeDetails = self.cartridgeDetails
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
    
    @IBAction func removeCartridge(_ sender: Any?) {
        
        if let emulator = Atari800Emulator.shared() {
         
            emulator.removeCartridge({ (ok, error) in
            
            });
        }
    }
    
    @IBAction func tvSystemChanged(_ sender: Any?) {
        
        let menuItem = sender as? NSMenuItem;
        
        if let emulator = Atari800Emulator.shared() {
            
            emulator.setIsNTSC(!emulator.isNTSC, completion: { (ok, error) in

                if let menuItem = menuItem {
                    
                    for otherMenuItem in menuItem.menu?.items ?? [] {
                
                        if otherMenuItem != menuItem {
                        
                            otherMenuItem.state = NSControl.StateValue.off
                        }
                    }
                    
                    menuItem.state = NSControl.StateValue.on
                }
            })
        }
    }
    
    @IBAction func showDrives(_ sender: Any?) {
        
        self.performSegue(withIdentifier: EmulationViewController.ShowDrivesSegue, sender: sender)
    }
}
