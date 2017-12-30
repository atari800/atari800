//
//  EmulationWindowController.swift
//  Atari800macOS
//
//  Created by Rod Münch on 25/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

import Cocoa

class EmulationWindowController: NSWindowController {

    var keysDown = Set<UInt16>()
    
    override func keyDown(with event: NSEvent) {
        
        keysDown.insert(event.keyCode)
    }
    
    override func keyUp(with event: NSEvent) {
        
        keysDown.remove(event.keyCode)
    }
    
    override func windowDidLoad() {
    
        super.windowDidLoad()
    }
}
