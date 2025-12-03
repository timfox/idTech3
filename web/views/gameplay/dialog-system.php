<?php
/**
 * Dialog System Documentation
 */
$title = 'Dialog System - id Tech 3 Documentation';
$breadcrumbs = [
    '/gameplay' => 'Gameplay',
    '/gameplay/dialog-system' => 'Dialog System'
];
?>

<div class="content-section">
    <h1>Dialog System</h1>
    
    <blockquote>
        <strong>Text-Based Conversation Interface:</strong> The dialog system provides a text-based conversation interface for displaying multi-page dialogs with character names and optional player choices. Dialogs appear at the bottom of the screen in a semi-transparent box, similar to classic RPG dialog systems.
    </blockquote>

    <div class="section">
        <h2>Overview</h2>
        <p>The dialog system provides a text-based conversation interface for displaying multi-page dialogs with character names and optional player choices. Dialogs appear at the bottom of the screen in a semi-transparent box, similar to classic RPG dialog systems.</p>
        
        <div class="feature-list">
            <h3>Features</h3>
            <ul>
                <li><strong>Multi-page dialogs:</strong> Support for up to 16 pages per dialog</li>
                <li><strong>Character names:</strong> Display speaker names above dialog text</li>
                <li><strong>Player choices:</strong> Up to 4 choices per page with entity triggers</li>
                <li><strong>Word wrapping:</strong> Automatic text wrapping for long messages</li>
                <li><strong>Entity integration:</strong> Trigger dialogs from map entities or programmatically</li>
            </ul>
        </div>
    </div>

    <div class="section">
        <h2>Map-Based Dialogs</h2>
        
        <h3>Creating a Dialog Entity</h3>
        <p>To create a dialog in your map, place a <code>target_dialog</code> entity:</p>
        <div class="code-block">
            <pre><code>{
"classname" "target_dialog"
"message" "Hello, traveler! Welcome to our village."
"message2" "Village Elder"
"targetname" "dialog_village_elder"
"target" "trigger_after_dialog"
}</code></pre>
        </div>

        <h3>Entity Properties</h3>
        <ul>
            <li><strong><code>message</code>:</strong> The dialog text to display (required)</li>
            <li><strong><code>message2</code>:</strong> The speaker's name (optional, displays above text)</li>
            <li><strong><code>targetname</code>:</strong> Unique name for referencing this entity (optional)</li>
            <li><strong><code>target</code>:</strong> Entity to trigger when dialog completes (optional)</li>
        </ul>

        <h3>Activating Dialogs</h3>
        <p>Dialogs are activated when a player uses (presses USE key) the <code>target_dialog</code> entity. You can also trigger them programmatically or via other entities.</p>

        <h4>Example: Trigger dialog when player touches a trigger</h4>
        <div class="code-block">
            <pre><code>{
"classname" "trigger_multiple"
"target" "dialog_village_elder"
}

{
"classname" "target_dialog"
"targetname" "dialog_village_elder"
"message" "You have entered the sacred chamber."
"message2" "Ancient Voice"
}</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Programmatic Dialog Creation</h2>
        
        <h3>Basic Dialog</h3>
        <div class="code-block">
            <pre><code>// Create a simple single-page dialog
int dialogId = G_Dialog_Create( clientNum, "NPC Name", "Hello! This is a dialog." );
G_Dialog_Show( dialogId, clientNum );</code></pre>
        </div>

        <h3>Multi-Page Dialog</h3>
        <div class="code-block">
            <pre><code>// Create a multi-page dialog
int dialogId = G_Dialog_Create( clientNum, "Guide", "Welcome to the tutorial." );
G_Dialog_AddPage( dialogId, "Guide", "This is page 2 of the tutorial." );
G_Dialog_AddPage( dialogId, "Guide", "And this is page 3. Press USE to continue." );
G_Dialog_Show( dialogId, clientNum );</code></pre>
        </div>

        <h3>Dialog with Choices</h3>
        <div class="code-block">
            <pre><code>// Create a dialog with choices
int dialogId = G_Dialog_Create( clientNum, "Merchant", "Would you like to buy something?" );
G_Dialog_AddChoice( dialogId, 0, "Yes, show me your wares", merchantEntityNum );
G_Dialog_AddChoice( dialogId, 0, "No, thank you", -1 );
G_Dialog_Show( dialogId, clientNum );</code></pre>
        </div>

        <h3>Complete Example</h3>
        <div class="code-block">
            <pre><code>void ShowQuestDialog( gentity_t *player, gentity_t *questGiver ) {
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
}</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Player Interaction</h2>
        
        <h3>Advancing Dialogs</h3>
        <ul>
            <li><strong>USE key:</strong> Press USE to advance to the next page (when no choices are present)</li>
            <li><strong>Console command:</strong> Type <code>dialognext</code> to advance manually</li>
            <li><strong>Automatic:</strong> Dialog closes automatically when the last page is reached</li>
        </ul>

        <h3>Selecting Choices</h3>
        <ul>
            <li><strong>Number keys:</strong> Press 1-4 to select the corresponding choice</li>
            <li><strong>Console command:</strong> Type <code>dialogchoice &lt;number&gt;</code> where number is 1-4</li>
        </ul>

        <h3>Closing Dialogs</h3>
        <p>Dialogs automatically close when:</p>
        <ul>
            <li>The last page is reached and advanced</li>
            <li>A choice is selected (if it's the last page)</li>
            <li>The player disconnects</li>
            <li>The server closes it programmatically</li>
        </ul>
    </div>

    <div class="section">
        <h2>Server-Side API</h2>
        
        <h3>Functions</h3>
        
        <h4><code>G_Dialog_Init()</code></h4>
        <p>Initialize the dialog system. Called automatically during level start.</p>

        <h4><code>G_Dialog_Shutdown()</code></h4>
        <p>Clean up all dialogs. Called automatically during level shutdown.</p>

        <h4><code>int G_Dialog_Create( int clientNum, const char *speaker, const char *text )</code></h4>
        <p>Create a new dialog.</p>
        <p><strong>Parameters:</strong></p>
        <ul>
            <li><code>clientNum</code>: Client number to show dialog to</li>
            <li><code>speaker</code>: Speaker name (can be NULL)</li>
            <li><code>text</code>: Dialog text for first page</li>
        </ul>
        <p><strong>Returns:</strong> Dialog ID (or -1 on error)</p>

        <h4><code>void G_Dialog_AddPage( int dialogId, const char *speaker, const char *text )</code></h4>
        <p>Add a page to an existing dialog.</p>
        <p><strong>Parameters:</strong></p>
        <ul>
            <li><code>dialogId</code>: Dialog ID from <code>G_Dialog_Create()</code></li>
            <li><code>speaker</code>: Speaker name for this page (can be NULL)</li>
            <li><code>text</code>: Dialog text for this page</li>
        </ul>

        <h4><code>void G_Dialog_AddChoice( int dialogId, int pageNum, const char *text, int target )</code></h4>
        <p>Add a choice to a dialog page.</p>
        <p><strong>Parameters:</strong></p>
        <ul>
            <li><code>dialogId</code>: Dialog ID</li>
            <li><code>pageNum</code>: Page number (0-based)</li>
            <li><code>text</code>: Choice text to display</li>
            <li><code>target</code>: Entity number to trigger when selected (-1 for no trigger)</li>
        </ul>

        <h4><code>void G_Dialog_Show( int dialogId, int clientNum )</code></h4>
        <p>Show a dialog to a client.</p>
        <p><strong>Parameters:</strong></p>
        <ul>
            <li><code>dialogId</code>: Dialog ID</li>
            <li><code>clientNum</code>: Client number to show dialog to</li>
        </ul>

        <h4><code>void G_Dialog_Close( int clientNum )</code></h4>
        <p>Close the active dialog for a client.</p>
        <p><strong>Parameters:</strong></p>
        <ul>
            <li><code>clientNum</code>: Client number</li>
        </ul>

        <h4><code>void G_Dialog_NextPage( int clientNum )</code></h4>
        <p>Advance to the next page of the active dialog.</p>
        <p><strong>Parameters:</strong></p>
        <ul>
            <li><code>clientNum</code>: Client number</li>
        </ul>

        <h4><code>void G_Dialog_SelectChoice( int clientNum, int choiceNum )</code></h4>
        <p>Select a choice in the current dialog page.</p>
        <p><strong>Parameters:</strong></p>
        <ul>
            <li><code>clientNum</code>: Client number</li>
            <li><code>choiceNum</code>: Choice number (0-based)</li>
        </ul>
    </div>

    <div class="section">
        <h2>Client-Side API</h2>
        
        <h3>Functions</h3>
        
        <h4><code>CG_Dialog_Init()</code></h4>
        <p>Initialize the client-side dialog system.</p>

        <h4><code>CG_Dialog_Shutdown()</code></h4>
        <p>Clean up the dialog system.</p>

        <h4><code>void CG_Dialog_Show( int id, int pageNum, const char *speaker, const char *text, int numChoices, const char *choices[], const int targets[] )</code></h4>
        <p>Show a dialog page (called automatically by server).</p>

        <h4><code>void CG_Dialog_Close()</code></h4>
        <p>Close the current dialog.</p>

        <h4><code>void CG_Dialog_Draw()</code></h4>
        <p>Draw the dialog box (called automatically each frame).</p>

        <h4><code>qboolean CG_Dialog_IsActive()</code></h4>
        <p>Check if a dialog is currently active.</p>
        <p><strong>Returns:</strong> <code>qtrue</code> if dialog is active, <code>qfalse</code> otherwise</p>
    </div>

    <div class="section">
        <h2>Limitations</h2>
        <ul>
            <li>Maximum 16 pages per dialog</li>
            <li>Maximum 4 choices per page</li>
            <li>Maximum 256 characters per dialog text</li>
            <li>Maximum 64 characters per speaker name</li>
            <li>Maximum 64 characters per choice text</li>
            <li>Maximum 32 active dialogs per level</li>
        </ul>
    </div>

    <div class="section">
        <h2>Examples</h2>
        
        <h3>Example 1: Simple Welcome Dialog</h3>
        <p><strong>Map entity:</strong></p>
        <div class="code-block">
            <pre><code>{
"classname" "target_dialog"
"message" "Welcome to our server! Have fun!"
"message2" "Server Admin"
}</code></pre>
        </div>

        <h3>Example 2: Tutorial Dialog with Multiple Pages</h3>
        <p><strong>Map entities:</strong></p>
        <div class="code-block">
            <pre><code>{
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
}</code></pre>
        </div>

        <h3>Example 3: Quest Dialog with Choices</h3>
        <p><strong>Map entity:</strong></p>
        <div class="code-block">
            <pre><code>{
"classname" "target_dialog"
"targetname" "quest_dialog"
"message" "I need your help! Will you accept my quest?"
"message2" "Quest Giver"
"target" "trigger_quest_accepted"
}</code></pre>
        </div>

        <p><strong>In code:</strong></p>
        <div class="code-block">
            <pre><code>void ShowQuestDialog( gentity_t *player, gentity_t *questGiver ) {
    int clientNum = player - g_entities;
    int dialogId;
    
    dialogId = G_Dialog_Create( clientNum, "Quest Giver", 
                                "I need your help! Will you accept my quest?" );
    G_Dialog_AddChoice( dialogId, 0, "Yes, I accept!", questGiver->target_ent->s.number );
    G_Dialog_AddChoice( dialogId, 0, "Tell me more", -1 );
    G_Dialog_AddChoice( dialogId, 0, "No, thanks", -1 );
    
    G_Dialog_Show( dialogId, clientNum );
}</code></pre>
        </div>

        <h3>Example 4: Conditional Dialog Based on Player State</h3>
        <div class="code-block">
            <pre><code>void ShowConditionalDialog( gentity_t *player ) {
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
}</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Tips and Best Practices</h2>
        <ol>
            <li><strong>Keep text concise:</strong> Long paragraphs may be cut off or hard to read</li>
            <li><strong>Use speaker names:</strong> Helps players identify who is talking</li>
            <li><strong>Limit choices:</strong> Too many choices can overwhelm players (max 4)</li>
            <li><strong>Test dialogs:</strong> Make sure text fits in the dialog box</li>
            <li><strong>Use entity targets:</strong> Link choices to game events for interactivity</li>
            <li><strong>Close dialogs properly:</strong> Always close dialogs when done to free resources</li>
            <li><strong>Handle errors:</strong> Check return values from dialog functions</li>
        </ol>
    </div>

    <div class="section">
        <h2>Troubleshooting</h2>
        
        <h3>Dialog doesn't appear</h3>
        <ul>
            <li>Check that <code>G_Dialog_Show()</code> was called with valid parameters</li>
            <li>Verify the client number is correct</li>
            <li>Ensure the dialog system was initialized (<code>G_Dialog_Init()</code>)</li>
        </ul>

        <h3>Text is cut off</h3>
        <ul>
            <li>Reduce text length (max 256 characters per page)</li>
            <li>Break long text into multiple pages</li>
            <li>Check for special characters that might cause issues</li>
        </ul>

        <h3>Choices don't work</h3>
        <ul>
            <li>Verify choice numbers are 0-3 (not 1-4)</li>
            <li>Check that target entity numbers are valid</li>
            <li>Ensure <code>G_Dialog_SelectChoice()</code> is being called correctly</li>
        </ul>

        <h3>Dialog doesn't close</h3>
        <ul>
            <li>Call <code>G_Dialog_Close()</code> manually if needed</li>
            <li>Check that all pages have been advanced</li>
            <li>Verify client is still connected</li>
        </ul>
    </div>

    <div class="section">
        <h2>Technical Details</h2>
        
        <h3>Dialog Structure</h3>
        <div class="code-block">
            <pre><code>typedef struct dialog_page {
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
} dialog_t;</code></pre>
        </div>

        <h3>Server Commands</h3>
        <p>The dialog system uses these server commands:</p>
        <ul>
            <li><code>dialog &lt;id&gt; &lt;page&gt; &lt;speaker&gt; &lt;text&gt; &lt;numChoices&gt; [choices...]</code>: Show dialog page</li>
            <li><code>dialogclose</code>: Close dialog</li>
        </ul>

        <h3>Client Commands</h3>
        <p>Players can use these console commands:</p>
        <ul>
            <li><code>dialognext</code>: Advance to next page</li>
            <li><code>dialogchoice &lt;num&gt;</code>: Select choice (1-4)</li>
        </ul>
    </div>

    <div class="section">
        <h2>Related Documentation</h2>
        <ul>
            <li><a href="gameplay/gameplay">Gameplay Systems</a> - Core gameplay mechanics</li>
            <li><a href="core/entity-system">Entity System</a> - Entity management</li>
            <li><a href="development/scripting">Scripting</a> - Game logic scripting</li>
            <li><a href="development/map-making">Map Making</a> - Creating maps with entities</li>
        </ul>
    </div>
</div>

