# Dialog System Documentation

## Overview

The dialog system provides a text-based conversation interface for displaying multi-page dialogs with character names and optional player choices. Dialogs appear at the bottom of the screen in a semi-transparent box, similar to classic RPG dialog systems.

## Features

- **Multi-page dialogs**: Support for up to 16 pages per dialog
- **Character names**: Display speaker names above dialog text
- **Player choices**: Up to 4 choices per page with entity triggers
- **Word wrapping**: Automatic text wrapping for long messages
- **Entity integration**: Trigger dialogs from map entities or programmatically

## Map-Based Dialogs

### Creating a Dialog Entity

To create a dialog in your map, place a `target_dialog` entity:

```
{
"classname" "target_dialog"
"message" "Hello, traveler! Welcome to our village."
"message2" "Village Elder"
"targetname" "dialog_village_elder"
"target" "trigger_after_dialog"
}
```

### Entity Properties

- **`message`**: The dialog text to display (required)
- **`message2`**: The speaker's name (optional, displays above text)
- **`targetname`**: Unique name for referencing this entity (optional)
- **`target`**: Entity to trigger when dialog completes (optional)

### Activating Dialogs

Dialogs are activated when a player uses (presses USE key) the `target_dialog` entity. You can also trigger them programmatically or via other entities.

**Example: Trigger dialog when player touches a trigger**

```
{
"classname" "trigger_multiple"
"target" "dialog_village_elder"
}

{
"classname" "target_dialog"
"targetname" "dialog_village_elder"
"message" "You have entered the sacred chamber."
"message2" "Ancient Voice"
}
```

## Programmatic Dialog Creation

### Basic Dialog

```c
// Create a simple single-page dialog
int dialogId = G_Dialog_Create( clientNum, "NPC Name", "Hello! This is a dialog." );
G_Dialog_Show( dialogId, clientNum );
```

### Multi-Page Dialog

```c
// Create a multi-page dialog
int dialogId = G_Dialog_Create( clientNum, "Guide", "Welcome to the tutorial." );
G_Dialog_AddPage( dialogId, "Guide", "This is page 2 of the tutorial." );
G_Dialog_AddPage( dialogId, "Guide", "And this is page 3. Press USE to continue." );
G_Dialog_Show( dialogId, clientNum );
```

### Dialog with Choices

```c
// Create a dialog with choices
int dialogId = G_Dialog_Create( clientNum, "Merchant", "Would you like to buy something?" );
G_Dialog_AddChoice( dialogId, 0, "Yes, show me your wares", merchantEntityNum );
G_Dialog_AddChoice( dialogId, 0, "No, thank you", -1 );
G_Dialog_Show( dialogId, clientNum );
```

### Complete Example

```c
void ShowQuestDialog( gentity_t *player, gentity_t *questGiver ) {
    int clientNum = player - g_entities;
    int dialogId;
    
    // Create dialog
    dialogId = G_Dialog_Create( clientNum, questGiver->message2, 
                                "I have a quest for you, brave warrior!" );
    
    // Add second page
    G_Dialog_AddPage( dialogId, questGiver->message2, 
                      "Will you help me defeat the evil dragon?" );
    
    // Add choices to second page
    G_Dialog_AddChoice( dialogId, 1, "Yes, I'll help!", questGiver->target_ent->s.number );
    G_Dialog_AddChoice( dialogId, 1, "Maybe later", -1 );
    G_Dialog_AddChoice( dialogId, 1, "No way!", -1 );
    
    // Show the dialog
    G_Dialog_Show( dialogId, clientNum );
}
```

## Player Interaction

### Advancing Dialogs

- **USE key**: Press USE to advance to the next page (when no choices are present)
- **Console command**: Type `dialognext` to advance manually
- **Automatic**: Dialog closes automatically when the last page is reached

### Selecting Choices

- **Number keys**: Press 1-4 to select the corresponding choice
- **Console command**: Type `dialogchoice <number>` where number is 1-4

### Closing Dialogs

Dialogs automatically close when:
- The last page is reached and advanced
- A choice is selected (if it's the last page)
- The player disconnects
- The server closes it programmatically

## Server-Side API

### Functions

#### `G_Dialog_Init()`
Initialize the dialog system. Called automatically during level start.

#### `G_Dialog_Shutdown()`
Clean up all dialogs. Called automatically during level shutdown.

#### `int G_Dialog_Create( int clientNum, const char *speaker, const char *text )`
Create a new dialog.

**Parameters:**
- `clientNum`: Client number to show dialog to
- `speaker`: Speaker name (can be NULL)
- `text`: Dialog text for first page

**Returns:** Dialog ID (or -1 on error)

#### `void G_Dialog_AddPage( int dialogId, const char *speaker, const char *text )`
Add a page to an existing dialog.

**Parameters:**
- `dialogId`: Dialog ID from `G_Dialog_Create()`
- `speaker`: Speaker name for this page (can be NULL)
- `text`: Dialog text for this page

#### `void G_Dialog_AddChoice( int dialogId, int pageNum, const char *text, int target )`
Add a choice to a dialog page.

**Parameters:**
- `dialogId`: Dialog ID
- `pageNum`: Page number (0-based)
- `text`: Choice text to display
- `target`: Entity number to trigger when selected (-1 for no trigger)

#### `void G_Dialog_Show( int dialogId, int clientNum )`
Show a dialog to a client.

**Parameters:**
- `dialogId`: Dialog ID
- `clientNum`: Client number to show dialog to

#### `void G_Dialog_Close( int clientNum )`
Close the active dialog for a client.

**Parameters:**
- `clientNum`: Client number

#### `void G_Dialog_NextPage( int clientNum )`
Advance to the next page of the active dialog.

**Parameters:**
- `clientNum`: Client number

#### `void G_Dialog_SelectChoice( int clientNum, int choiceNum )`
Select a choice in the current dialog page.

**Parameters:**
- `clientNum`: Client number
- `choiceNum`: Choice number (0-based)

## Client-Side API

### Functions

#### `CG_Dialog_Init()`
Initialize the client-side dialog system.

#### `CG_Dialog_Shutdown()`
Clean up the dialog system.

#### `void CG_Dialog_Show( int id, int pageNum, const char *speaker, const char *text, int numChoices, const char *choices[], const int targets[] )`
Show a dialog page (called automatically by server).

#### `void CG_Dialog_Close()`
Close the current dialog.

#### `void CG_Dialog_Draw()`
Draw the dialog box (called automatically each frame).

#### `qboolean CG_Dialog_IsActive()`
Check if a dialog is currently active.

**Returns:** `qtrue` if dialog is active, `qfalse` otherwise

## Limitations

- Maximum 16 pages per dialog
- Maximum 4 choices per page
- Maximum 256 characters per dialog text
- Maximum 64 characters per speaker name
- Maximum 64 characters per choice text
- Maximum 32 active dialogs per level

## Examples

### Example 1: Simple Welcome Dialog

**Map entity:**
```
{
"classname" "target_dialog"
"message" "Welcome to our server! Have fun!"
"message2" "Server Admin"
}
```

### Example 2: Tutorial Dialog with Multiple Pages

**Map entities:**
```
{
"classname" "target_dialog"
"targetname" "tutorial_dialog"
"message" "Welcome to the tutorial. Let's learn the basics."
"message2" "Tutorial Guide"
}

{
"classname" "target_dialog"
"targetname" "tutorial_dialog"
"message" "First, learn to move with WASD keys."
"message2" "Tutorial Guide"
}

{
"classname" "target_dialog"
"targetname" "tutorial_dialog"
"message" "Press SPACE to jump. Good luck!"
"message2" "Tutorial Guide"
}
```

### Example 3: Quest Dialog with Choices

**Map entity:**
```
{
"classname" "target_dialog"
"targetname" "quest_dialog"
"message" "I need your help! Will you accept my quest?"
"message2" "Quest Giver"
"target" "trigger_quest_accepted"
}
```

**In code:**
```c
void ShowQuestDialog( gentity_t *player, gentity_t *questGiver ) {
    int clientNum = player - g_entities;
    int dialogId;
    
    dialogId = G_Dialog_Create( clientNum, "Quest Giver", 
                                "I need your help! Will you accept my quest?" );
    G_Dialog_AddChoice( dialogId, 0, "Yes, I accept!", questGiver->target_ent->s.number );
    G_Dialog_AddChoice( dialogId, 0, "Tell me more", -1 );
    G_Dialog_AddChoice( dialogId, 0, "No, thanks", -1 );
    
    G_Dialog_Show( dialogId, clientNum );
}
```

### Example 4: Conditional Dialog Based on Player State

```c
void ShowConditionalDialog( gentity_t *player ) {
    int clientNum = player - g_entities;
    int dialogId;
    
    if( player->client->ps.stats[STAT_HEALTH] < 50 ) {
        dialogId = G_Dialog_Create( clientNum, "Medic", 
                                    "You're hurt! Let me heal you." );
        G_Dialog_AddChoice( dialogId, 0, "Yes, please heal me", medicEntityNum );
        G_Dialog_AddChoice( dialogId, 0, "I'm fine", -1 );
    } else {
        dialogId = G_Dialog_Create( clientNum, "Medic", 
                                    "You look healthy! Come back if you need healing." );
    }
    
    G_Dialog_Show( dialogId, clientNum );
}
```

## Tips and Best Practices

1. **Keep text concise**: Long paragraphs may be cut off or hard to read
2. **Use speaker names**: Helps players identify who is talking
3. **Limit choices**: Too many choices can overwhelm players (max 4)
4. **Test dialogs**: Make sure text fits in the dialog box
5. **Use entity targets**: Link choices to game events for interactivity
6. **Close dialogs properly**: Always close dialogs when done to free resources
7. **Handle errors**: Check return values from dialog functions

## Troubleshooting

### Dialog doesn't appear
- Check that `G_Dialog_Show()` was called with valid parameters
- Verify the client number is correct
- Ensure the dialog system was initialized (`G_Dialog_Init()`)

### Text is cut off
- Reduce text length (max 256 characters per page)
- Break long text into multiple pages
- Check for special characters that might cause issues

### Choices don't work
- Verify choice numbers are 0-3 (not 1-4)
- Check that target entity numbers are valid
- Ensure `G_Dialog_SelectChoice()` is being called correctly

### Dialog doesn't close
- Call `G_Dialog_Close()` manually if needed
- Check that all pages have been advanced
- Verify client is still connected

## Technical Details

### Dialog Structure

```c
typedef struct dialog_page {
    char text[ MAX_DIALOG_TEXT ];              // 256 chars
    char speaker[ MAX_DIALOG_NAME ];          // 64 chars
    int numChoices;                            // 0-4
    dialog_choice_t choices[ MAX_DIALOG_CHOICES ];
} dialog_page_t;

typedef struct dialog {
    int id;                                    // Unique ID
    int numPages;                              // 1-16
    dialog_page_t pages[ MAX_DIALOG_PAGES ];
    int currentPage;                           // 0-based
    qboolean active;                           // Is dialog active?
    int clientNum;                             // Client viewing dialog
} dialog_t;
```

### Server Commands

The dialog system uses these server commands:
- `dialog <id> <page> <speaker> <text> <numChoices> [choices...]`: Show dialog page
- `dialogclose`: Close dialog

### Client Commands

Players can use these console commands:
- `dialognext`: Advance to next page
- `dialogchoice <num>`: Select choice (1-4)

## See Also

- `g_dialog.c` / `g_dialog.h`: Server-side implementation
- `cg_dialog.c` / `cg_dialog.h`: Client-side implementation
- `g_spawn.c`: Entity spawn system integration

