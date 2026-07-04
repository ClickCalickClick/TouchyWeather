// Custom Clay component: drag-to-reorder card list ("cardorder").
//
// Renders the 10 toggleable cards as rows with a ≡ grip; dragging the grip
// reorders rows (touch + mouse). The component's value is a CSV of ToggleIds
// in visual order (e.g. "9,0,1,2,3,4,5,6,7,8") — the exact format the watch
// pushes in its card-state seed and decodes back in comm.c
// (settings_apply_order_csv). Main and Settings are not listed: Main is
// always first, Settings always last.
//
// IMPORTANT: Clay serializes this whole object into the config page with
// toSource(), so every function below must be self-contained — no closures
// over this module's (or PKJS's) scope. The row-label table is therefore
// duplicated inside set(). ES5 only (old phone webviews).
module.exports = {
  name: 'cardorder',
  template: [
    '<div class="component component-cardorder">',
    '  <span class="label">{{{label}}}</span>',
    '  <div class="cardorder-list" data-manipulator-target></div>',
    '  {{if description}}<div class="description">{{{description}}}</div>{{/if}}',
    '</div>'
  ].join('\n'),
  style: [
    '.component-cardorder { padding-bottom: 6px; }',
    '.component-cardorder .label { display: block; padding: 10px 10px 6px; }',
    '.component-cardorder .cardorder-row {',
    '  position: relative;',
    '  display: block;',
    '  padding: 10px;',
    '  background: rgba(255,255,255,0.06);',
    '  border-bottom: 1px solid rgba(0,0,0,0.28);',
    '  color: inherit;',
    '}',
    '.component-cardorder .cardorder-row.cardorder-dragging {',
    '  z-index: 10;',
    '  background: rgba(255,255,255,0.2);',
    '  box-shadow: 0 2px 8px rgba(0,0,0,0.45);',
    '}',
    '.component-cardorder .cardorder-grip {',
    '  float: right;',
    '  padding: 0 6px 0 18px;',
    '  color: rgba(255,255,255,0.55);',
    '  cursor: move;',
    '  touch-action: none;',
    '  -webkit-user-select: none;',
    '  user-select: none;',
    '}'
  ].join('\n'),
  defaults: {
    label: '',
    description: ''
  },
  manipulator: {
    get: function() {
      var list = this.$manipulatorTarget[0];
      var ids = [];
      for (var i = 0; i < list.children.length; i++) {
        var tid = list.children[i].getAttribute('data-tid');
        if (tid !== null) ids.push(tid);
      }
      return ids.join(',');
    },
    set: function(value) {
      // ToggleId → label (C enum order: HOURS..RADAR=8, ADVICE=9).
      var labels = ['6 Hours', 'Week Ahead', 'Rain / Precipitation', 'UV Index',
                    'Air Quality', 'Sun Cycle', 'Night Sky', 'Golden Hour',
                    'Radar', 'Touch & Go'];
      // Validate: exactly one of each id 0..9, else fall back to the default
      // order (Touch & Go first, matching the watch's s_default_order).
      var order = String(value || '').split(',');
      var seen = {};
      var valid = (order.length === labels.length);
      for (var i = 0; valid && i < order.length; i++) {
        var v = parseInt(order[i], 10);
        if (isNaN(v) || v < 0 || v >= labels.length || seen[v]) valid = false;
        seen[v] = true;
        order[i] = v;
      }
      if (!valid) order = [9, 0, 1, 2, 3, 4, 5, 6, 7, 8];

      if (this.get() === order.join(',')) { return this; }

      var list = this.$manipulatorTarget[0];
      while (list.firstChild) list.removeChild(list.firstChild);
      for (var j = 0; j < order.length; j++) {
        var row = document.createElement('div');
        row.className = 'cardorder-row';
        row.setAttribute('data-tid', String(order[j]));
        var grip = document.createElement('span');
        grip.className = 'cardorder-grip';
        grip.appendChild(document.createTextNode('≡'));
        var name = document.createElement('span');
        name.className = 'cardorder-name';
        name.appendChild(document.createTextNode(labels[order[j]]));
        row.appendChild(grip);
        row.appendChild(name);
        list.appendChild(row);
      }
      return this.trigger('change');
    }
  },
  initialize: function(minified, clayConfig) {
    var self = this;
    var list = self.$manipulatorTarget[0];
    var drag = null;

    function pointY(e) {
      if (e.touches && e.touches.length) return e.touches[0].clientY;
      if (e.changedTouches && e.changedTouches.length) {
        return e.changedTouches[0].clientY;
      }
      return e.clientY;
    }
    function inClass(t, cls) {
      while (t && t !== list) {
        if (t.className && String(t.className).indexOf(cls) >= 0) return t;
        t = t.parentNode;
      }
      return null;
    }
    function setShift(row, px) {
      var v = px ? 'translateY(' + px + 'px)' : '';
      row.style.webkitTransform = v;
      row.style.transform = v;
    }
    function start(e) {
      if (drag) return;
      if (!inClass(e.target, 'cardorder-grip')) return;
      var row = inClass(e.target, 'cardorder-row');
      if (!row) return;
      drag = { row: row, startY: pointY(e) };
      row.className += ' cardorder-dragging';
      e.preventDefault();
    }
    function move(e) {
      if (!drag) return;
      e.preventDefault();
      var row = drag.row;
      var y = pointY(e);
      var dy = y - drag.startY;
      // Swap with a neighbour once the drag passes 60% of its height; loop so
      // a fast flick can cross several rows in one move event.
      var swapped = true;
      while (swapped) {
        swapped = false;
        var next = row.nextElementSibling;
        var prev = row.previousElementSibling;
        if (next && dy > next.offsetHeight * 0.6) {
          row.parentNode.insertBefore(next, row);
          drag.startY += next.offsetHeight;
          swapped = true;
        } else if (prev && dy < -prev.offsetHeight * 0.6) {
          row.parentNode.insertBefore(row, prev);
          drag.startY -= prev.offsetHeight;
          swapped = true;
        }
        dy = y - drag.startY;
      }
      setShift(row, dy);
    }
    function end() {
      if (!drag) return;
      var row = drag.row;
      row.className = String(row.className).replace(/ *cardorder-dragging/g, '');
      setShift(row, 0);
      drag = null;
      self.trigger('change');
    }

    list.addEventListener('touchstart', start, false);
    list.addEventListener('touchmove', move, false);
    list.addEventListener('touchend', end, false);
    list.addEventListener('touchcancel', end, false);
    list.addEventListener('mousedown', start, false);
    document.addEventListener('mousemove', move, false);
    document.addEventListener('mouseup', end, false);
  }
};
