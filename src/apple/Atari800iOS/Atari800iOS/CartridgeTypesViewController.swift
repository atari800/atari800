//
//  CartridgeTypesViewController.swift
//  Atari800iOS
//
//  Created by Rod Münch on 30/12/2017.
//  Copyright © 2017 Atari800 development team. All rights reserved.
//

import UIKit
import Atari800EmulationCore

class CartridgeTypesViewController: UITableViewController {

    var cartridgeSize: Int = 16
    var cartridgeFileName: String?
    
    var cartridgeTypes: [NSNumber: String]?
    var sortedCartridgeTypes: [NSNumber]?
    
    var cartridgeCompletion: Atari800CartridgeSelectionHandler? = nil
    
    override func viewDidLoad() {
        
        cartridgeTypes = Atari800Emulator.shared().supportedCartridgeTypes(forSize: self.cartridgeSize)
        sortedCartridgeTypes =  cartridgeTypes?.keys.sorted(by: { (a, b) -> Bool in
            
            return a.compare(b) == .orderedAscending
        })
            
        super.viewDidLoad()
    }
    
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
    
        return sortedCartridgeTypes?.count ?? 0
    }
    
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        
        let cell = tableView.dequeueReusableCell(withIdentifier: "Cartridge Type", for: indexPath)
        
        if let key = sortedCartridgeTypes?[indexPath.row] {
            
            cell.textLabel?.text = cartridgeTypes![key]
        }
        
        return cell
    }
    
    func dismiss(_ ok: Bool, type: Int) {
        
        self.navigationController?.presentingViewController?.dismiss(animated: true, completion: {
            
            self.cartridgeCompletion?(ok, type)
        })
    }
    
    @IBAction func ok(_ sender: Any?) {
        
        if let selectedPath = self.tableView.indexPathsForSelectedRows?.first {
            
            if let selectedType = sortedCartridgeTypes?[selectedPath.row] {
                
                self.dismiss(true, type: selectedType.intValue)
            }
        }
    }
    
    @IBAction func cancel(_ sender: Any?) {
        
        self.dismiss(false, type: 0)
    }
}
