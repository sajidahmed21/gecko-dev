/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * @fileOverview
 * This module is used for storing changes to settings that are
 * requested by extensions, and for finding out what the current value
 * of a setting should be, based on the precedence chain.
 *
 * When multiple extensions request to make a change to a particular
 * setting, the most recently installed extension will be given
 * precedence.
 *
 * This precedence chain of settings is stored in JSON format,
 * without indentation, using UTF-8 encoding.
 * With indentation applied, the file would look like this:
 *
 * {
 *   type: { // The type of settings being stored in this object, i.e., prefs.
 *     key: { // The unique key for the setting.
 *       initialValue, // The initial value of the setting.
 *       precedenceList: [
 *         {
 *           id, // The id of the extension requesting the setting.
 *           installDate, // The install date of the extension.
 *           value // The value of the setting requested by the extension.
 *         }
 *       ],
 *     },
 *     key: {
 *       // ...
 *     }
 *   }
 * }
 *
 */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionSettingsStore"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "JSONFile",
                                  "resource://gre/modules/JSONFile.jsm");

const JSON_FILE_NAME = "extension-settings.json";
const STORE_PATH = OS.Path.join(Services.dirsvc.get("ProfD", Ci.nsIFile).path, JSON_FILE_NAME);

let _store;

// Get the internal settings store, which is persisted in a JSON file.
async function getStore(type) {
  if (!_store) {
    _store = new JSONFile({
      path: STORE_PATH,
    });
    await _store.load();
  }
  _store.ensureDataReady();

  // Ensure a property exists for the given type.
  if (!_store.data[type]) {
    _store.data[type] = {};
  }

  return _store;
}

// Return an object with properties for key and value|initialValue, or null
// if no setting has been stored for that key.
async function getTopItem(type, key) {
  let store = await getStore(type);

  let keyInfo = store.data[type][key];
  if (!keyInfo) {
    return null;
  }

  if (!keyInfo.precedenceList.length) {
    return {key, initialValue: keyInfo.initialValue};
  }

  return {key, value: keyInfo.precedenceList[0].value};
}

this.ExtensionSettingsStore = {
  /**
   * Adds a setting to the store, possibly returning the current top precedent
   * setting.
   *
   * @param {Extension} extension
   *        The extension for which a setting is being added.
   * @param {string} type
   *        The type of setting to be stored.
   * @param {string} key
   *        A string that uniquely identifies the setting.
   * @param {string} value
   *        The value to be stored in the setting.
   * @param {function} initialValueCallback
   *        An function to be called to determine the initial value for the
   *        setting. This will be passed the value in the callbackArgument
   *        argument.
   * @param {any} callbackArgument
   *        The value to be passed into the initialValueCallback. It defaults to
   *        the value of the key argument.
   *
   * @returns {object | null} Either an object with properties for key and
   *                          value, which corresponds to the item that was
   *                          just added, or null if the item that was just
   *                          added does not need to be set because it is not
   *                          at the top of the precedence list.
   */
  async addSetting(extension, type, key, value, initialValueCallback, callbackArgument = key) {
    if (typeof initialValueCallback != "function") {
      throw new Error("initialValueCallback must be a function.");
    }

    let id = extension.id;
    let store = await getStore(type);

    if (!store.data[type][key]) {
      // The setting for this key does not exist. Set the initial value.
      let initialValue = await initialValueCallback(callbackArgument);
      store.data[type][key] = {
        initialValue,
        precedenceList: [],
      };
    }
    let keyInfo = store.data[type][key];
    // Check for this item in the precedenceList.
    let foundIndex = keyInfo.precedenceList.findIndex(item => item.id == id);
    if (foundIndex == -1) {
      // No item for this extension, so add a new one.
      let addon = await AddonManager.getAddonByID(id);
      keyInfo.precedenceList.push({id, installDate: addon.installDate, value});
    } else {
      // Item already exists or this extension, so update it.
      keyInfo.precedenceList[foundIndex].value = value;
    }

    // Sort the list.
    keyInfo.precedenceList.sort((a, b) => {
      return b.installDate - a.installDate;
    });

    store.saveSoon();

    // Check whether this is currently the top item.
    if (keyInfo.precedenceList[0].id == id) {
      return {key, value};
    }
    return null;
  },

  /**
   * Removes a setting from the store, returning the current top precedent
   * setting.
   *
   * @param {Extension} extension The extension for which a setting is being removed.
   * @param {string} type The type of setting to be removed.
   * @param {string} key A string that uniquely identifies the setting.
   *
   * @returns {object | null} Either an object with properties for key and
   *                          value, which corresponds to the current top
   *                          precedent setting, or null if the current top
   *                          precedent setting has not changed.
   */
  async removeSetting(extension, type, key) {
    let returnItem;
    let store = await getStore(type);

    let keyInfo = store.data[type][key];
    if (!keyInfo) {
      throw new Error(
        `Cannot remove setting for ${type}:${key} as it does not exist.`);
    }

    let id = extension.id;
    let foundIndex = keyInfo.precedenceList.findIndex(item => item.id == id);

    if (foundIndex == -1) {
      throw new Error(
        `Cannot remove setting for ${type}:${key} as it does not exist.`);
    }

    keyInfo.precedenceList.splice(foundIndex, 1);

    if (foundIndex == 0) {
      returnItem = await getTopItem(type, key);
    }

    if (keyInfo.precedenceList.length == 0) {
      delete store.data[type][key];
    }
    store.saveSoon();

    return returnItem;
  },

  /**
   * Retrieves all settings from the store for a given extension.
   *
   * @param {Extension} extension The extension for which a settings are being retrieved.
   * @param {string} type The type of setting to be returned.
   *
   * @returns {array} A list of settings which have been stored for the extension.
   */
  async getAllForExtension(extension, type) {
    let store = await getStore(type);

    let keysObj = store.data[type];
    let items = [];
    for (let key in keysObj) {
      if (keysObj[key].precedenceList.find(item => item.id == extension.id)) {
        items.push(key);
      }
    }
    return items;
  },

  /**
   * Retrieves a setting from the store, returning the current top precedent
   * setting for the key.
   *
   * @param {string} type The type of setting to be returned.
   * @param {string} key A string that uniquely identifies the setting.
   *
   * @returns {object} An object with properties for key and value.
   */
  async getSetting(type, key) {
    return await getTopItem(type, key);
  },

  /**
   * Return the levelOfControl for a key / extension combo.
   * levelOfControl is required by Google's ChromeSetting prototype which
   * in turn is used by the privacy API among others.
   *
   * It informs a caller of the state of a setting with respect to the current
   * extension, and can be one of the following values:
   *
   * controlled_by_other_extensions: controlled by extensions with higher precedence
   * controllable_by_this_extension: can be controlled by this extension
   * controlled_by_this_extension: controlled by this extension
   *
   * @param {Extension} extension The extension for which levelOfControl is being
   *                              requested.
   * @param {string} type The type of setting to be returned. For example `pref`.
   * @param {string} key A string that uniquely identifies the setting, for
   *                     example, a preference name.
   *
   * @returns {string} The level of control of the extension over the key.
   */
  async getLevelOfControl(extension, type, key) {
    let store = await getStore(type);

    let keyInfo = store.data[type][key];
    if (!keyInfo || !keyInfo.precedenceList.length) {
      return "controllable_by_this_extension";
    }

    let id = extension.id;
    let topItem = keyInfo.precedenceList[0];
    if (topItem.id == id) {
      return "controlled_by_this_extension";
    }

    let addon = await AddonManager.getAddonByID(id);
    return topItem.installDate > addon.installDate ?
      "controlled_by_other_extensions" :
      "controllable_by_this_extension";
  },
};
