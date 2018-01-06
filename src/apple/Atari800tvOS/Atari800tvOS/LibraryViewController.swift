//
//  LibraryViewController.swift
//  Atari800tvOS
//
//  Created by Rod Münch on 06/01/2018.
//  Copyright © 2018 Atari800 development team. All rights reserved.
//

import UIKit

class LibraryViewController: UIViewController {

    @IBAction func startEmulation(_ sender: Any?) {
        
        self.performSegue(withIdentifier: "Start Emulation", sender: sender)
    }
}
