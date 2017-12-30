//
//  CartridgeTypesViewController.swift
//  Atari800macOS
//
//  Created by Simon Lawrence on 29/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

import Cocoa
import Atari800EmulationCore_macOS

class CartridgeTypesViewController: NSViewController {

    var cartridgeSize: Int = 16
    var cartridgeFileName: String?
    
    var cartridgeTypes: [NSNumber: String]?
    var sortedCartridgeTypes: [NSNumber]?
    var cartridgeCompletion: Atari800CartridgeSelectionHandler? = nil
    
    @IBOutlet weak var tableView: NSTableView?
    @IBOutlet weak var prompt: NSTextField?
    
    override func viewDidLoad() {
    
        cartridgeTypes = Atari800Emulator.shared().supportedCartridgeTypes(forSize: self.cartridgeSize)
        sortedCartridgeTypes =  cartridgeTypes?.keys.sorted(by: { (a, b) -> Bool in
            return a.compare(b) == .orderedAscending
        }
        if let prompt = self.prompt {
            
            prompt.stringValue = String(format: "Select the cartridge type for %@:", self.cartridgeFileName ?? "(Unknown)");
        }
        super.viewDidLoad()
    }
    
    @IBAction func ok(_ sender: Any?) {
        
        if let cartridgeCompletion = cartridgeCompletion {
            
            if let selectedRow = tableView?.selectedRow {
            
                let key = sortedCartridgeTypes![selectedRow]
                
                cartridgeCompletion(true, key.intValue)
            }
        }
        
        self.dismiss(sender);
    }
    
    @IBAction func cancel(_ sender: Any?) {
     
        if let cartridgeCompletion = cartridgeCompletion {
            
            cartridgeCompletion(false, 0)
        }
        
        self.dismiss(sender);
    }
}

extension CartridgeTypesViewController: NSTableViewDataSource {
    
    func numberOfRows(in tableView: NSTableView) -> Int {
        
        return cartridgeTypes?.count ?? 0
    }
    
    func tableView(_ tableView: NSTableView, objectValueFor tableColumn: NSTableColumn?, row: Int) -> Any? {
        
        if let key = sortedCartridgeTypes?[row] {
            
            return cartridgeTypes![key]
        }
        
        return nil
    }
}
