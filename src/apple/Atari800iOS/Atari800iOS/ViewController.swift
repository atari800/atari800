//
//  ViewController.swift
//  Atari800iOS
//
//  Created by Simon Lawrence on 19/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

import UIKit
import Atari800EmulationCore_iOS

class ViewController: UIViewController {

    let width = 384
    let height = 240
    
    weak var mtkView: MTKView?
    var renderer: Atari800Renderer?
    
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
            
            let renderer = Atari800Renderer(metalKitView: mtkView, widthInPixels: width, heightInScanLines: height)
            
            mtkView.delegate = renderer
            self.renderer = renderer
            self.updateDisplay(self)
            renderer.mtkView(mtkView, drawableSizeWillChange: mtkView.drawableSize)
        }
    }
    
    static var o: Int = 0;
    
    @IBAction func updateDisplay(_ sender: Any?) {
        
        if let screen = renderer?.screen {
            
            var i = 0;
            
            for y: Int in 0..<height {
                
                var c: Int = ((y + ViewController.o) / 16) * 16;
                c = c & 0xFF
                
                for _ in 0..<width {
                    
                    screen[i] = UInt32(c)
                    c += 1
                    
                    c = c & 0xFF
                    
                    i += 1
                }
            }
            
            ViewController.o += 8;
        }
    }

    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
    }


}

