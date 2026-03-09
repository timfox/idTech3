/**
 * Options tab switching and ui_open_tab handling.
 * Copy to your mod's ui/options_tabs.js and ensure handleOpenTab is called
 * from the main menu's onOpen.
 */

var UI_OPEN_TAB_CVAR = 'ui_open_tab';

var TAB_TO_MENU = {
  graphics: 'options_menu',
  audio: 'options_audio_menu',
  gameplay: 'options_gameplay_menu',
  advanced: 'options_advanced_menu',
  bindings: 'options_bindings_menu'
};

function switchTab(tab) {
  var menu = TAB_TO_MENU[String(tab).toLowerCase()];
  if (!menu) {
    return;
  }
  idtech3.exec('close options_menu ; close options_advanced_menu ; close options_audio_menu ; close options_gameplay_menu ; close options_bindings_menu ; open ' + menu);
}

function handleOpenTab() {
  var tab = (idtech3.cvarGet(UI_OPEN_TAB_CVAR) || '').toLowerCase();
  if (!tab) {
    return;
  }

  var cmd = '';
  if (tab === 'credits') {
    cmd = 'close main ; open credits_menu';
  } else if (tab === 'audio') {
    cmd = 'close main ; open options_audio_menu';
  } else if (tab === 'gameplay') {
    cmd = 'close main ; open options_gameplay_menu';
  }

  if (cmd) {
    idtech3.exec(cmd + ' ; seta ' + UI_OPEN_TAB_CVAR + ' ""');
  }
}

module.exports = {
  switchTab: switchTab,
  handleOpenTab: handleOpenTab
};
