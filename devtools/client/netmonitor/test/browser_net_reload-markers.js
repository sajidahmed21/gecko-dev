/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the empty-requests reload button works.
 */

add_task(function* () {
  let { monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, windowRequire } = monitor.panelWin;
  let { EVENTS } = windowRequire("devtools/client/netmonitor/constants");
  let button = document.querySelector("#requests-menu-reload-notice-button");
  button.click();

  let markers = [];

  monitor.panelWin.on(EVENTS.TIMELINE_EVENT, (_, marker) => {
    markers.push(marker);
  });

  yield waitForNetworkEvents(monitor, 1);
  yield waitUntil(() => markers.length == 2);

  ok(true, "Reloading finished");

  is(markers[0].name, "document::DOMContentLoaded",
    "The first received marker is correct.");
  is(markers[1].name, "document::Load",
    "The second received marker is correct.");

  return teardown(monitor);
});
