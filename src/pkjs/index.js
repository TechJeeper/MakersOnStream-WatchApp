// Phone side: Clay settings + fetch MakersOnStream live/upcoming for the watch.

var Clay = require('@rebble/clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var API_BASE = 'https://makersonstream.app/api/v1';
var STORE_KEY = 'mos.watch.settings';
var MAX_LIVE = 8;
var MAX_SOON = 16;

var CMD_REFRESH = 1;
var CMD_BEGIN = 2;
var CMD_ITEM = 3;
var CMD_DONE = 4;
var CMD_ERROR = 5;

var KIND_LIVE = 0;
var KIND_SOON = 1;

var DEFAULTS = {
  apiKey: '',
  mode: 'all', // 'following' | 'all'
  days: 2
};

var settings = null;
var sending = false;
var queue = [];

function load() {
  try {
    var raw = localStorage.getItem(STORE_KEY);
    if (raw) return Object.assign({}, DEFAULTS, JSON.parse(raw));
  } catch (e) {}
  return Object.assign({}, DEFAULTS);
}

function save(s) {
  try {
    localStorage.setItem(STORE_KEY, JSON.stringify(s));
  } catch (e) {}
}

function clamp(s) {
  return {
    apiKey: String(s.apiKey || '').trim(),
    mode: s.mode === 'following' ? 'following' : 'all',
    days: Math.max(1, Math.min(7, parseInt(s.days, 10) || 2))
  };
}

function trunc(str, n) {
  str = String(str || '').replace(/\s+/g, ' ').trim();
  if (str.length <= n) return str;
  return str.slice(0, n - 1) + '…';
}

function platformLabel(p) {
  if (!p) return '';
  p = String(p).toLowerCase();
  if (p === 'twitch') return 'Twitch';
  if (p === 'youtube') return 'YouTube';
  if (p === 'tiktok') return 'TikTok';
  return trunc(p, 10);
}

function parseWhen(isoLike) {
  if (!isoLike) return null;
  var s = String(isoLike).trim();
  if (/^\d{4}-\d{2}-\d{2} /.test(s)) {
    s = s.replace(' ', 'T') + 'Z';
  }
  var d = new Date(s);
  return isNaN(d.getTime()) ? null : d;
}

function formatWhen(isoLike) {
  var d = parseWhen(isoLike);
  if (!d) return '';
  var now = Date.now();
  var diff = d.getTime() - now;
  var abs = Math.abs(diff);
  var mins = Math.round(abs / 60000);
  if (diff < 0 && mins < 45) return 'started';
  if (diff >= 0 && mins < 60) return 'in ' + Math.max(1, mins) + 'm';
  var hours = Math.round(abs / 3600000);
  if (diff >= 0 && hours < 24) return 'in ' + hours + 'h';
  var opts = { weekday: 'short', hour: 'numeric', minute: '2-digit' };
  try {
    return d.toLocaleString(undefined, opts);
  } catch (e) {
    return trunc(isoLike, 20);
  }
}

function ymd(d) {
  var y = d.getUTCFullYear();
  var m = ('0' + (d.getUTCMonth() + 1)).slice(-2);
  var day = ('0' + d.getUTCDate()).slice(-2);
  return y + '-' + m + '-' + day;
}

function authHeaders() {
  var h = { Accept: 'application/json' };
  if (settings.apiKey) {
    h['Authorization'] = 'Bearer ' + settings.apiKey;
    h['X-API-Key'] = settings.apiKey;
  }
  return h;
}

function fetchJson(path) {
  return new Promise(function (resolve, reject) {
    var req = new XMLHttpRequest();
    req.open('GET', API_BASE + path, true);
    var headers = authHeaders();
    Object.keys(headers).forEach(function (k) {
      req.setRequestHeader(k, headers[k]);
    });
    req.timeout = 15000;
    req.onload = function () {
      if (req.status >= 200 && req.status < 300) {
        try {
          resolve(JSON.parse(req.responseText));
        } catch (e) {
          reject(new Error('Bad JSON'));
        }
      } else if (req.status === 401) {
        reject(new Error('Bad API key'));
      } else {
        reject(new Error('HTTP ' + req.status));
      }
    };
    req.onerror = function () { reject(new Error('Network error')); };
    req.ontimeout = function () { reject(new Error('Timed out')); };
    req.send();
  });
}

function flushQueue() {
  if (sending || queue.length === 0) return;
  sending = true;
  var msg = queue.shift();
  Pebble.sendAppMessage(msg, function () {
    sending = false;
    flushQueue();
  }, function () {
    sending = false;
    setTimeout(flushQueue, 400);
  });
}

function enqueue(msg) {
  queue.push(msg);
  flushQueue();
}

function sendError(text) {
  queue = [];
  sending = false;
  enqueue({ Cmd: CMD_ERROR, Status: trunc(text, 36) });
}

function sendFeed(liveItems, soonItems, mode) {
  queue = [];
  sending = false;
  enqueue({
    Cmd: CMD_BEGIN,
    LiveCount: liveItems.length,
    SoonCount: soonItems.length,
    Mode: mode === 'following' ? 0 : 1
  });

  liveItems.forEach(function (item, i) {
    enqueue({
      Cmd: CMD_ITEM,
      Kind: KIND_LIVE,
      Index: i,
      Name: trunc(item.name, 26),
      Subtitle: trunc(item.subtitle, 38),
      When: trunc(item.when, 22),
      Platform: trunc(item.platform, 10)
    });
  });

  soonItems.forEach(function (item, i) {
    enqueue({
      Cmd: CMD_ITEM,
      Kind: KIND_SOON,
      Index: i,
      Name: trunc(item.name, 26),
      Subtitle: trunc(item.subtitle, 38),
      When: trunc(item.when, 22),
      Platform: trunc(item.platform, 10)
    });
  });

  enqueue({ Cmd: CMD_DONE });
}

function mapLive(creators) {
  return (creators || []).slice(0, MAX_LIVE).map(function (c) {
    var cats = (c.categories || []).map(function (x) { return x.name; }).filter(Boolean);
    var subtitle = c.current_status || c.project_title || cats[0] || 'Live now';
    var plat = (c.platforms && c.platforms[0]) || '';
    return {
      id: c.id || 0,
      name: c.display_name || c.slug || 'Maker',
      subtitle: subtitle,
      when: 'LIVE',
      platform: platformLabel(plat)
    };
  });
}

function mapEvents(events, excludeLiveIds) {
  var skip = excludeLiveIds || {};
  var now = Date.now();
  var out = [];
  (events || []).forEach(function (e) {
    if (out.length >= MAX_SOON) return;
    if (e.is_live_now) return;
    var start = parseWhen(e.occurrence_at || e.start_at);
    if (!start || start.getTime() < now - 30 * 60000) return;
    if (e.creator_id && skip[e.creator_id]) return;
    out.push({
      name: e.creator_name || 'Maker',
      subtitle: e.title || 'Stream',
      when: formatWhen(e.occurrence_at || e.start_at),
      platform: platformLabel(e.platform || (e.platforms && e.platforms[0]))
    });
  });
  return out;
}

function refresh() {
  if (!settings) settings = clamp(load());
  var mode = settings.mode;
  if (mode === 'following' && !settings.apiKey) {
    sendError('Add API key in Settings');
    return;
  }

  var from = new Date();
  var to = new Date(from.getTime() + settings.days * 24 * 3600 * 1000);
  var range = '?from=' + encodeURIComponent(ymd(from)) +
              '&to=' + encodeURIComponent(ymd(to));

  var livePromise;
  var eventsPromise;

  if (mode === 'following') {
    livePromise = fetchJson('/following').then(function (data) {
      var followed = (data.following || []).filter(function (c) { return c.is_live; });
      return mapLive(followed);
    });
    eventsPromise = fetchJson('/following/events' + range).then(function (data) {
      return data.events || [];
    });
  } else {
    livePromise = fetchJson('/live').then(function (data) {
      return mapLive(data.live || []);
    });
    eventsPromise = fetchJson('/events' + range).then(function (data) {
      return data.events || [];
    });
  }

  Promise.all([livePromise, eventsPromise]).then(function (parts) {
    var liveItems = parts[0];
    var liveIds = {};
    liveItems.forEach(function (item) {
      if (item.id) liveIds[item.id] = true;
    });
    var soonItems = mapEvents(parts[1], liveIds);
    sendFeed(liveItems, soonItems, mode);
  }).catch(function (err) {
    sendError((err && err.message) || 'Fetch failed');
  });
}

function clayValue(dict, key, fallback) {
  if (!dict || dict[key] === undefined || dict[key] === null) return fallback;
  // Clay may return { value: ... } objects depending on convert mode.
  if (typeof dict[key] === 'object' && dict[key].value !== undefined) {
    return dict[key].value;
  }
  return dict[key];
}

Pebble.addEventListener('ready', function () {
  settings = clamp(load());
  refresh();
});

Pebble.addEventListener('appmessage', function (e) {
  var dict = e.payload || {};
  if (dict.Cmd === CMD_REFRESH) {
    refresh();
  }
});

Pebble.addEventListener('showConfiguration', function () {
  if (!settings) settings = clamp(load());
  clay.setSettings({
    FeedMode: settings.mode,
    ApiKey: settings.apiKey,
    Days: String(settings.days)
  });
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  var dict = clay.getSettings(e.response, false);
  settings = clamp({
    apiKey: clayValue(dict, 'ApiKey', settings.apiKey),
    mode: clayValue(dict, 'FeedMode', settings.mode),
    days: clayValue(dict, 'Days', settings.days)
  });
  save(settings);
  refresh();
});
