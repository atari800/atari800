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
    
    static let ShowCartridgeTypesSegue = "Show Cartridge Types"
    
    var cartridgeSize: Int = 0
    var cartridgeFileName: String?
    var cartridgeCompletion: Atari800CartridgeSelectionHandler? = nil
    
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
                
                emulator.delegate = self
                emulator.startEmulation()
            }
        }
    }
    
    @IBAction func openDocument(_ sender: Any?) {
        
        let picker = UIDocumentPickerViewController(documentTypes: ["org.atari.xex", "org.atari.rom"], in: .import)
        
        picker.delegate = self
        picker.modalTransitionStyle = .crossDissolve
        picker.modalPresentationStyle = .fullScreen
        
        self.present(picker, animated: true) {
            
        }
    }
    
    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        
        if segue.identifier == EmulationViewController.ShowCartridgeTypesSegue {
            
            if let navigationController = segue.destination as? UINavigationController {
                if let cartridgeTypesController = navigationController.viewControllers.first as? CartridgeTypesViewController {
                    
                    cartridgeTypesController.cartridgeFileName = self.cartridgeFileName
                    cartridgeTypesController.cartridgeCompletion = self.cartridgeCompletion
                    cartridgeTypesController.cartridgeSize = self.cartridgeSize
                }
            }
        }
    }
}

extension EmulationViewController: Atari800EmulatorDelegate {
    
    func emulator(_ emulator: Atari800Emulator!, didSelectCartridgeWithSize size: Int, filename: String!, completion: Atari800CartridgeSelectionHandler!) {
        
        self.cartridgeFileName = filename
        self.cartridgeCompletion = completion
        self.cartridgeSize = size
        
        self.performSegue(withIdentifier: EmulationViewController.ShowCartridgeTypesSegue, sender: self)
    }
}

extension EmulationViewController: UIDocumentPickerDelegate {
    
    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
        
        if let url = urls.first, let emulator = Atari800Emulator.shared() {
         
            emulator.loadFile(url, completion: { (ok, error) in
                
            })
        }
    }
}
