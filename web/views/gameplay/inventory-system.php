<?php
/**
 * Inventory System Documentation
 */
$title = 'Inventory System - id Tech 3 Documentation';
$breadcrumbs = [
    '/gameplay' => 'Gameplay',
    '/gameplay/inventory-system' => 'Inventory System'
];
?>

<div class="content-section">
    <h1>Inventory System</h1>
    
    <blockquote>
        <strong>Comprehensive Item Management:</strong> The inventory system provides a comprehensive item management system with persistent storage, equipment slots with stat modifiers, and a crafting system. Players can collect items, equip them for bonuses, and combine items to create new ones.
    </blockquote>

    <div class="section">
        <h2>Overview</h2>
        <p>The inventory system provides a comprehensive item management system with persistent storage, equipment slots with stat modifiers, and a crafting system. Players can collect items, equip them for bonuses, and combine items to create new ones.</p>
        
        <div class="feature-list">
            <h3>Features</h3>
            <ul>
                <li><strong>Item Management:</strong> Store up to 256 different items with quantities</li>
                <li><strong>Persistent Storage:</strong> Inventory is saved to disk and persists across sessions</li>
                <li><strong>Equipment System:</strong> Equip items to slots for stat bonuses (damage, accuracy, armor, health)</li>
                <li><strong>Crafting System:</strong> Combine two items to create new items using recipes</li>
                <li><strong>Client UI:</strong> Overlay menu for viewing and managing inventory</li>
            </ul>
        </div>
    </div>

    <div class="section">
        <h2>Server-Side Components</h2>
        
        <h3>Core Files</h3>
        <ul>
            <li><code>g_inventory.c/h</code> - Core inventory management</li>
            <li><code>g_equipment.c/h</code> - Equipment definitions and stat modifiers</li>
            <li><code>g_crafting.h</code> - Crafting recipe system (implemented in g_inventory.c)</li>
        </ul>

        <h3>Data Structures</h3>
        
        <h4>Inventory Item</h4>
        <div class="code-block">
            <pre><code>typedef struct {
    int itemId;              // Item ID
    int quantity;            // Quantity owned
    int durability;          // Durability (-1 for infinite)
    inventory_item_type_t type;
    char name[MAX_ITEM_NAME];
    char description[MAX_ITEM_DESCRIPTION];
    char icon[MAX_ITEM_ICON_PATH];
} inventory_item_t;</code></pre>
        </div>

        <h4>Equipment Stats</h4>
        <div class="code-block">
            <pre><code>typedef struct {
    float damage_multiplier;  // Damage multiplier (1.0 = no change)
    float rof_multiplier;      // Rate of fire multiplier
    float accuracy_bonus;      // Accuracy bonus (0.0 = no change)
    int armor_bonus;          // Armor bonus points
    int health_bonus;         // Health bonus points
} equipment_stats_t;</code></pre>
        </div>

        <h3>Equipment Slots</h3>
        <ul>
            <li><code>EQUIP_SLOT_WEAPON_MOD</code> (0) - Weapon modifications</li>
            <li><code>EQUIP_SLOT_ARMOR_HELMET</code> (1) - Helmet armor</li>
            <li><code>EQUIP_SLOT_ARMOR_VEST</code> (2) - Vest/chest armor</li>
            <li><code>EQUIP_SLOT_ARMOR_LEGS</code> (3) - Leg armor</li>
        </ul>
    </div>

    <div class="section">
        <h2>API Functions</h2>
        
        <h3>Inventory Management</h3>
        <ul>
            <li><code>G_Inventory_AddItem(clientNum, itemId, quantity)</code> - Add item to inventory</li>
            <li><code>G_Inventory_RemoveItem(clientNum, itemId, quantity)</code> - Remove item from inventory</li>
            <li><code>G_Inventory_HasItem(clientNum, itemId, quantity)</code> - Check if player has item</li>
            <li><code>G_Inventory_GetItemCount(clientNum, itemId)</code> - Get quantity of item</li>
            <li><code>G_Inventory_Load(clientNum)</code> - Load inventory from file</li>
            <li><code>G_Inventory_Save(clientNum)</code> - Save inventory to file</li>
        </ul>

        <h3>Equipment</h3>
        <ul>
            <li><code>G_Inventory_EquipItem(clientNum, slot, itemId)</code> - Equip item to slot</li>
            <li><code>G_Inventory_UnequipItem(clientNum, slot)</code> - Unequip item from slot</li>
            <li><code>G_Inventory_GetTotalEquipmentStats(clientNum)</code> - Get combined stat modifiers</li>
        </ul>

        <h3>Crafting</h3>
        <ul>
            <li><code>G_Crafting_RegisterRecipe(input1, input2, output, outputQuantity, name)</code> - Register recipe</li>
            <li><code>G_Crafting_CanCraft(clientNum, itemId1, itemId2)</code> - Check if can craft</li>
            <li><code>G_Crafting_Craft(clientNum, itemId1, itemId2)</code> - Craft items</li>
        </ul>
    </div>

    <div class="section">
        <h2>Client-Side Components</h2>
        
        <h3>Files</h3>
        <ul>
            <li><code>cg_inventory.c/h</code> - Client-side inventory UI</li>
        </ul>

        <h3>UI Functions</h3>
        <ul>
            <li><code>CG_Inventory_Toggle()</code> - Toggle inventory menu</li>
            <li><code>CG_Inventory_Draw()</code> - Draw inventory overlay</li>
            <li><code>CG_Inventory_HandleInput(key)</code> - Handle keyboard input</li>
            <li><code>CG_Inventory_Update(...)</code> - Update display data from server</li>
        </ul>

        <h3>Controls</h3>
        <ul>
            <li><strong>TAB</strong> - Toggle inventory menu</li>
            <li><strong>Number keys (1-9)</strong> - Select items</li>
            <li><strong>ENTER</strong> - Use/equip selected item</li>
            <li><strong>ESCAPE</strong> - Close inventory</li>
        </ul>
    </div>

    <div class="section">
        <h2>Server Commands</h2>
        
        <h3>Player Commands</h3>
        <ul>
            <li><code>inventory</code> - Display inventory to client</li>
            <li><code>inventoryuse &lt;itemId&gt;</code> - Use an item</li>
            <li><code>inventoryequip &lt;slot&gt; &lt;itemId&gt;</code> - Equip item to slot</li>
            <li><code>inventoryunequip &lt;slot&gt;</code> - Unequip item from slot</li>
            <li><code>inventorycraft &lt;itemId1&gt; &lt;itemId2&gt;</code> - Craft items</li>
        </ul>
    </div>

    <div class="section">
        <h2>Configuration Files</h2>
        
        <h3>Equipment Definitions (<code>inventory/equipment.dat</code>)</h3>
        <p><strong>Format:</strong></p>
        <div class="code-block">
            <pre><code>equipment &lt;itemId&gt; &lt;slot&gt; &lt;damage_mult&gt; &lt;rof_mult&gt; &lt;accuracy_bonus&gt; &lt;armor_bonus&gt; &lt;health_bonus&gt; "&lt;name&gt;" "&lt;description&gt;"</code></pre>
        </div>

        <p><strong>Example:</strong></p>
        <div class="code-block">
            <pre><code>equipment 1000 0 1.0 1.0 0.1 0 0 "Weapon Scope" "Increases accuracy by 10%"
equipment 2001 2 1.0 1.0 0.0 25 0 "Armor Plate" "Adds 25 armor points"</code></pre>
        </div>

        <h3>Crafting Recipes (<code>inventory/recipes.dat</code>)</h3>
        <p><strong>Format:</strong></p>
        <div class="code-block">
            <pre><code>recipe &lt;input1&gt; &lt;input2&gt; &lt;output&gt; &lt;outputQuantity&gt; "&lt;name&gt;"</code></pre>
        </div>

        <p><strong>Example:</strong></p>
        <div class="code-block">
            <pre><code>recipe 1000 1001 1003 1 "Advanced Scope"
recipe 2000 2001 2003 1 "Full Armor Set"</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Inventory Storage</h2>
        <p>Inventories are saved to <code>inventory/&lt;GUID&gt;.inv</code> files. The file format is:</p>
        
        <div class="code-block">
            <pre><code>version 1
guid &lt;player_guid&gt;
items &lt;count&gt;
&lt;itemId&gt; &lt;quantity&gt; &lt;durability&gt;
...
equipment &lt;count&gt;
&lt;slot&gt; &lt;itemId&gt; &lt;damage_mult&gt; &lt;rof_mult&gt; &lt;accuracy_bonus&gt; &lt;armor_bonus&gt; &lt;health_bonus&gt;
...</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Integration Points</h2>
        
        <h3>Equipment Stat Modifiers</h3>
        <p>Equipment bonuses are automatically applied:</p>
        <ul>
            <li><strong>Damage:</strong> Applied in <code>G_Damage()</code> and weapon firing functions</li>
            <li><strong>Accuracy:</strong> Reduces weapon spread in <code>Bullet_Fire()</code></li>
            <li><strong>Armor:</strong> Added in <code>CheckArmor()</code> and spawn functions</li>
            <li><strong>Health:</strong> Added to max health and current health on spawn</li>
        </ul>

        <h3>Item Pickup Integration</h3>
        <p>To add items when picking up entities, call:</p>
        <div class="code-block">
            <pre><code>G_Inventory_AddItem( clientNum, itemId, quantity );</code></pre>
        </div>

        <h3>Client Connect/Disconnect</h3>
        <ul>
            <li>Inventory is loaded on <code>ClientConnect()</code> (if not a bot)</li>
            <li>Inventory is saved on <code>ClientDisconnect()</code></li>
        </ul>
    </div>

    <div class="section">
        <h2>Usage Examples</h2>
        
        <h3>Adding Items Programmatically</h3>
        <div class="code-block">
            <pre><code>// Add 5 of item ID 1000 to player
G_Inventory_AddItem( clientNum, 1000, 5 );

// Check if player has item
if( G_Inventory_HasItem( clientNum, 1000, 1 ) ) {
    // Player has at least 1 of item 1000
}</code></pre>
        </div>

        <h3>Equipping Items</h3>
        <div class="code-block">
            <pre><code>// Equip item 1000 to weapon mod slot
if( G_Inventory_EquipItem( clientNum, EQUIP_SLOT_WEAPON_MOD, 1000 ) ) {
    // Successfully equipped
}</code></pre>
        </div>

        <h3>Crafting</h3>
        <div class="code-block">
            <pre><code>// Check if can craft
if( G_Crafting_CanCraft( clientNum, 1000, 1001 ) ) {
    // Craft the items
    G_Crafting_Craft( clientNum, 1000, 1001 );
}</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Tips</h2>
        <ol>
            <li><strong>Item IDs:</strong> Use IDs >= 1000 for custom items to avoid conflicts with game items</li>
            <li><strong>Equipment Slots:</strong> Only one item per slot can be equipped at a time</li>
            <li><strong>Stat Modifiers:</strong> Multipliers are multiplied together, bonuses are added</li>
            <li><strong>Persistence:</strong> Inventories are saved automatically on disconnect</li>
            <li><strong>UI:</strong> The inventory UI can be toggled with TAB key</li>
        </ol>
    </div>

    <div class="section">
        <h2>Troubleshooting</h2>
        
        <h3>Inventory Not Saving</h3>
        <ul>
            <li>Check that <code>inventory/</code> directory exists</li>
            <li>Verify player has valid GUID</li>
            <li>Check file permissions</li>
        </ul>

        <h3>Equipment Not Working</h3>
        <ul>
            <li>Ensure equipment is defined in <code>equipment.dat</code></li>
            <li>Verify item ID matches equipment definition</li>
            <li>Check slot number is correct</li>
        </ul>

        <h3>Crafting Fails</h3>
        <ul>
            <li>Verify recipe exists in <code>recipes.dat</code></li>
            <li>Check player has both input items</li>
            <li>Ensure output item can be added to inventory</li>
        </ul>
    </div>

    <div class="section">
        <h2>Future Enhancements</h2>
        <ul>
            <li>Item icons and visual representations</li>
            <li>Item tooltips and descriptions</li>
            <li>Drag-and-drop UI</li>
            <li>Item categories and filtering</li>
            <li>Trading system between players</li>
            <li>Item durability system</li>
            <li>Item sets with bonuses</li>
        </ul>
    </div>

    <div class="section">
        <h2>Related Documentation</h2>
        <ul>
            <li><a href="gameplay/gameplay">Gameplay Systems</a> - Core gameplay mechanics</li>
            <li><a href="gameplay/dialog-system">Dialog System</a> - NPC interaction system</li>
            <li><a href="core/entity-system">Entity System</a> - Entity management</li>
            <li><a href="development/scripting">Scripting</a> - Game logic scripting</li>
        </ul>
    </div>
</div>

