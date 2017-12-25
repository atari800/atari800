//
//  EmulationViewController.swift
//  Atari800iOS
//
//  Created by Rod Münch on 19/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

import UIKit
import MetalKit
import Atari800EmulationCore_iOS
import AVFoundation;

class EmulationViewController: UIViewController {
    
    @IBOutlet weak var metalView: MTKView?
    @IBOutlet weak var quartzView: UIView?
    @IBOutlet weak var keyboardView: Atari800KeyboardView?
    
    let width = 384
    let height = 240
    
    override func viewDidLoad() {
        
        super.viewDidLoad()
        
        let session = AVAudioSession.sharedInstance()
        do {
            try session.setCategory(AVAudioSessionCategoryPlayback)
            try session.setActive(true)
        } catch let error as NSError {
            print("Unable to activate audio session:  \(error.localizedDescription)")
        }
        
        // Do any additional setup after loading the view.
        if let metalView = self.metalView {
            
            metalView.device = MTLCreateSystemDefaultDevice()
            metalView.preferredFramesPerSecond = 60
            
            var emulationView = self.quartzView
            
            if metalView.device == nil {
                
                print("Metal is not supported on this device.");
                metalView.removeFromSuperview()
                self.metalView = nil
            }
            else {
                
                emulationView = metalView
                self.quartzView?.removeFromSuperview()
            }
            
            if let emulator = Atari800Emulator.shared() {
                
                if let keyboardView = self.keyboardView {
                
                    emulator.keyboardHandler = keyboardView.newKeyboardHandler()
                }
                
                if let renderer = emulator.renderer {
                    
                    renderer.setup(forView: emulationView, widthInPixels: width, heightInScanLines: height);
                
                    self.metalView?.delegate?.mtkView(metalView, drawableSizeWillChange: metalView.drawableSize)
                }
                
                let driver = Atari800AudioDriver()
                
                emulator.audioDriver = driver;
                
                emulator.startEmulation()
            }
        }
    }
    
    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
    }
    
}
