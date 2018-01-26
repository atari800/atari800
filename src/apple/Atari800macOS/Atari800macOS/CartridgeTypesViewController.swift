//
//  CartridgeTypesViewController.swift
//  Atari800macOS
//
//  Created by Rod Münch on 29/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

import Cocoa
import Atari800EmulationCore

class CartridgeTypesViewController: NSViewController {

    var cartridgeDetails: CartridgeDetails?
    
    var sortedCartridgeTypes: [NSNumber]?
    
    @IBOutlet weak var tableView: NSTableView?
    @IBOutlet weak var prompt: NSTextField?
    
    override func viewDidLoad() {
    
        sortedCartridgeTypes =  cartridgeDetails?.types.keys.sorted(by: { (a, b) -> Bool in
            
            return a.compare(b) == .orderedAscending
        })
        
        if let prompt = self.prompt {
            
            prompt.stringValue = String(format: "Select the cartridge type for %@:", cartridgeDetails?.fileName ?? "(Unknown)");
        }
        
        super.viewDidLoad()
    }
    
    override func viewWillAppear() {
        
        super.viewWillAppear()
        
        self.tableView?.selectRowIndexes(IndexSet([0]), byExtendingSelection: false)
    }
    
    @IBAction func ok(_ sender: Any?) {
        
        if let cartridgeCompletion = cartridgeDetails?.completion {
            
            if let selectedRow = tableView?.selectedRow {
            
                let key = sortedCartridgeTypes![selectedRow]
                
                cartridgeCompletion(true, key.intValue)
            }
        }
        
        self.dismiss(sender);
    }
    
    @IBAction func cancel(_ sender: Any?) {
     
        if let cartridgeCompletion = cartridgeDetails?.completion {
            
            cartridgeCompletion(false, 0)
        }
        
        self.dismiss(sender);
    }
}

extension CartridgeTypesViewController: NSTableViewDataSource {
    
    func numberOfRows(in tableView: NSTableView) -> Int {
        
        return cartridgeDetails?.types.count ?? 0
    }
    
    func tableView(_ tableView: NSTableView, objectValueFor tableColumn: NSTableColumn?, row: Int) -> Any? {
        
        if let key = sortedCartridgeTypes?[row] {
            
            return cartridgeDetails!.types[key]
        }
        
        return nil
    }
}
