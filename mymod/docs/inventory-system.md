# Inventory System Documentation

## Overview

The inventory system provides a comprehensive item management system with persistent storage, equipment slots with stat modifiers, and a crafting system. Players can collect items, equip them for bonuses, and combine items to create new ones.

## Features

- **Item Management**: Store up to 256 different items with quantities
- **Persistent Storage**: Inventory is saved to disk and persists across sessions
- **Equipment System**: Equip items to slots for stat bonuses (damage, accuracy, armor, health)
- **Crafting System**: Combine two items to create new items using recipes
- **Client UI**: Overlay menu for viewing and managing inventory

## Server-Side Components

### Core Files

- `g_inventory.c/h` - Core inventory management
- `g_equipment.c/h` - Equipment definitions and stat modifiers
- `g_crafting.h` - Crafting recipe system (implemented in g_inventory.c)

### Data Structures

#### Inventory Item
```c
typedef struct {
    int itemId;              // Item ID
    int quantity;            // Quantity owned
    int durability;          // Durability (-1 for infinite)
    inventory_item_type_t type;
    char name[MAX_ITEM_NAME];
    char description[MAX_ITEM_DESCRIPTION];
    char icon[MAX_ITEM_ICON_PATH];
} inventory_item_t;
```

#### Equipment Stats
```c
typedef struct {
    float damage_multiplier;  // Damage multiplier (1.0 = no change)
    float rof_multiplier;      // Rate of fire multiplier
    float accuracy_bonus;      // Accuracy bonus (0.0 = no change)
    int armor_bonus;          // Armor bonus points
    int health_bonus;         // Health bonus points
} equipment_stats_t;
```

### Equipment Slots

- `EQUIP_SLOT_WEAPON_MOD` (0) - Weapon modifications
- `EQUIP_SLOT_ARMOR_HELMET` (1) - Helmet armor
- `EQUIP_SLOT_ARMOR_VEST` (2) - Vest/chest armor
- `EQUIP_SLOT_ARMOR_LEGS` (3) - Leg armor

### API Functions

#### Inventory Management
- `G_Inventory_AddItem(clientNum, itemId, quantity)` - Add item to inventory
- `G_Inventory_RemoveItem(clientNum, itemId, quantity)` - Remove item from inventory
- `G_Inventory_HasItem(clientNum, itemId, quantity)` - Check if player has item
- `G_Inventory_GetItemCount(clientNum, itemId)` - Get quantity of item
- `G_Inventory_Load(clientNum)` - Load inventory from file
- `G_Inventory_Save(clientNum)` - Save inventory to file

#### Equipment
- `G_Inventory_EquipItem(clientNum, slot, itemId)` - Equip item to slot
- `G_Inventory_UnequipItem(clientNum, slot)` - Unequip item from slot
- `G_Inventory_GetTotalEquipmentStats(clientNum)` - Get combined stat modifiers

#### Crafting
- `G_Crafting_RegisterRecipe(input1, input2, output, outputQuantity, name)` - Register recipe
- `G_Crafting_CanCraft(clientNum, itemId1, itemId2)` - Check if can craft
- `G_Crafting_Craft(clientNum, itemId1, itemId2)` - Craft items

## Client-Side Components

### Files

- `cg_inventory.c/h` - Client-side inventory UI

### UI Functions

- `CG_Inventory_Toggle()` - Toggle inventory menu
- `CG_Inventory_Draw()` - Draw inventory overlay
- `CG_Inventory_HandleInput(key)` - Handle keyboard input
- `CG_Inventory_Update(...)` - Update display data from server

### Controls

- **TAB** - Toggle inventory menu
- **Number keys (1-9)** - Select items
- **ENTER** - Use/equip selected item
- **ESCAPE** - Close inventory

## Server Commands

### Player Commands

- `inventory` - Display inventory to client
- `inventoryuse <itemId>` - Use an item
- `inventoryequip <slot> <itemId>` - Equip item to slot
- `inventoryunequip <slot>` - Unequip item from slot
- `inventorycraft <itemId1> <itemId2>` - Craft items

## Configuration Files

### Equipment Definitions (`inventory/equipment.dat`)

Format:
```
equipment <itemId> <slot> <damage_mult> <rof_mult> <accuracy_bonus> <armor_bonus> <health_bonus> "<name>" "<description>"
```

Example:
```
equipment 1000 0 1.0 1.0 0.1 0 0 "Weapon Scope" "Increases accuracy by 10%"
equipment 2001 2 1.0 1.0 0.0 25 0 "Armor Plate" "Adds 25 armor points"
```

### Crafting Recipes (`inventory/recipes.dat`)

Format:
```
recipe <input1> <input2> <output> <outputQuantity> "<name>"
```

Example:
```
recipe 1000 1001 1003 1 "Advanced Scope"
recipe 2000 2001 2003 1 "Full Armor Set"
```

## Inventory Storage

Inventories are saved to `inventory/<GUID>.inv` files. The file format is:

```
version 1
guid <player_guid>
items <count>
<itemId> <quantity> <durability>
...
equipment <count>
<slot> <itemId> <damage_mult> <rof_mult> <accuracy_bonus> <armor_bonus> <health_bonus>
...
```

## Integration Points

### Equipment Stat Modifiers

Equipment bonuses are automatically applied:

- **Damage**: Applied in `G_Damage()` and weapon firing functions
- **Accuracy**: Reduces weapon spread in `Bullet_Fire()`
- **Armor**: Added in `CheckArmor()` and spawn functions
- **Health**: Added to max health and current health on spawn

### Item Pickup Integration

To add items when picking up entities, call:
```c
G_Inventory_AddItem( clientNum, itemId, quantity );
```

### Client Connect/Disconnect

- Inventory is loaded on `ClientConnect()` (if not a bot)
- Inventory is saved on `ClientDisconnect()`

## Usage Examples

### Adding Items Programmatically

```c
// Add 5 of item ID 1000 to player
G_Inventory_AddItem( clientNum, 1000, 5 );

// Check if player has item
if( G_Inventory_HasItem( clientNum, 1000, 1 ) ) {
    // Player has at least 1 of item 1000
}
```

### Equipping Items

```c
// Equip item 1000 to weapon mod slot
if( G_Inventory_EquipItem( clientNum, EQUIP_SLOT_WEAPON_MOD, 1000 ) ) {
    // Successfully equipped
}
```

### Crafting

```c
// Check if can craft
if( G_Crafting_CanCraft( clientNum, 1000, 1001 ) ) {
    // Craft the items
    G_Crafting_Craft( clientNum, 1000, 1001 );
}
```

## Tips

1. **Item IDs**: Use IDs >= 1000 for custom items to avoid conflicts with game items
2. **Equipment Slots**: Only one item per slot can be equipped at a time
3. **Stat Modifiers**: Multipliers are multiplied together, bonuses are added
4. **Persistence**: Inventories are saved automatically on disconnect
5. **UI**: The inventory UI can be toggled with TAB key

## Troubleshooting

### Inventory Not Saving

- Check that `inventory/` directory exists
- Verify player has valid GUID
- Check file permissions

### Equipment Not Working

- Ensure equipment is defined in `equipment.dat`
- Verify item ID matches equipment definition
- Check slot number is correct

### Crafting Fails

- Verify recipe exists in `recipes.dat`
- Check player has both input items
- Ensure output item can be added to inventory

## Future Enhancements

- Item icons and visual representations
- Item tooltips and descriptions
- Drag-and-drop UI
- Item categories and filtering
- Trading system between players
- Item durability system
- Item sets with bonuses

