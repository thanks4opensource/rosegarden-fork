rosegarden-fork: Enhanced version of Rosegarden MIDI sequencer and musical notation editor
==========================================================================================

<a name="repository_description"></a>
A fork of [Rosegarden](#acknowledgment_and_motivation) with new
Matrix Editor, and Loop, Marker, and Chord Name Ruler, features and
improvements.

<br> <a name="contents"></a>
Contents
--------

* [License](#license)
* [Acknowledgment and motivation](#acknowledgment_and_motivation)
* [Build and installation](#build_and_installation)
* [New features](#new_features)
    * [Matrix/Percussion Editor unification](#percussion_matrix_editor_unification)
    * [Color](#color)
    * [Additional Main Window features](#additional_main_window_features)
    * [Matrix Editor](#matrix_editor)
        * [Features in all editing tools](#features_in_all_tools)
        * [Individual tools](#individual_tools)
        * [Other new features](#other_new_features)
    * [Loop Ruler](#loop_ruler)
    * [Marker Ruler](#marker_ruler)
    * [Chord Name Ruler](#chord_name_ruler)
* [Further information](#further_information)


<br> <a name="license"></a>
License
-------

Rosegarden is a MIDI sequencer and musical notation editor for Linux.

Copyright 2000-2022 The Rosegarden Development Team
<br>Modifications and additions Copyright &copy; 2022 Mark R. Rubin aka "thanks4opensource" aka "thanks4opensrc"

This file is part of Rosegarden.

The Rosegarden program is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 2 of the
License, or (at your option) any later version.

The Rosegarden program is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty
of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the [GNU General Public License,
version 2](COPYING) along with the Rosegarden program.  If not, see
<https://www.gnu.org/licenses/licenses.html>


<br> <a name="acknowledgment_and_motivation"></a>
Acknowledgment and motivation
------------------------------

This is a fork of Rosegarden, the official source code,
documentation, and downloads for which is available at:

* [https://rosegardenmusic.com/](https://rosegardenmusic.com/)
* [https://sourceforge.net/projects/rosegarden/](https://sourceforge.net/projects/rosegarden/)
* [https://sourceforge.net/p/rosegarden/git/ci/master/tree/](https://sourceforge.net/p/rosegarden/git/ci/master/tree/)
* [https://github.com/tedfelix/rosegarden-official](https://github.com/tedfelix/rosegarden-official)

Although the fork has many new features and improvements, the vast
majority of the code is unchanged from the official sources.
Rosegarden has an incredibly long and deep history (see [Origin of a
Legend](https://rosegardenmusic.com/resources/authors/)) representing
literally many man-centuries of work by dozens if not hundreds of
developers, several of whom (in particular the current lead
maintainer) have each devoted over a decade's worth of tireless
effort to Rosegarden. Their work is acknowledged and greatly
appreciated.

Note also that the fork's changes were submitted (and will continue
to be available) for merge into the official project. As of this
writing this has not happened, and in fact it seems unlikely that it
will, either in the foreseeable future or in fact if ever. This
author can only speculate as to why this is the case, but it has been
hinted that changes to Rosegarden's user interface and/or workflow
(beyond the most trivial) are unlikely to be approved based on a
belief that a large user base exists, the majority of whom would not
accept such changes regardless of whether they represent improvements
or not. There also seems to be a premium placed on program stability
(an understandable position, although in this case possibly taken to
an extreme) and changes of the magnitude as those included here
obviously risk undermining that.

Wherever possible, the forked code offers options to use its new
features or revert to the official version's UI and workflows. But
attempts to implement this were impractical in several cases, such as
when already overly complex code would have become too convoluted
(excessive `if (official_compatibility == true)` conditionals
scattered everywhere) or when doing so would have made no sense (e.g.
maintaining a separate/vestigal/duplicate Percussion Matrix Editor
identical to the new unified [Matrix
Editor](#percussion_matrix_editor_unification)).

Caution: As of this writing the fork has had testing solely by the
author, and doubtless contains many bugs. The intent is to fix them
as they are discovered and/or reported (within the caveats of the NO
WARRANTY disclaimers contained in the [license](#license)).
However ...

**Important:** Do not report any bugs in the fork to the official
Rosegarden sites listed above. The fork is the sole responsibility
its author and and failures in it in no way reflect upon the official
version's maintainers and developers. The only exception is for bugs
that have been separately, independently, and definitively
demonstrated as occurring in the official Rosegarden versions.

In any case, it is the author's belief that the fork may benefit
others (as it has him), and it is therefor offered for use herein.



<br> <a name="build_and_installation"></a>
Build and installation 
----------------------

The process of building and installing the code is almost identical
to the procedure described in
[README-official.md](README-official.md).

The only change is that the local repository must first be switched
to the `thanks4opensrc-devel` branch, i.e.:

    $ git checkout thanks4opensrc-devel

before continuing with:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make

For further information, see [README-official.md](README-official.md).


<br> <a name="new_features"></a>
New features
------------
As per this repository's description
[above](#repository_description), the majority of the new features
pertain to the Matrix Editor. Users who do not, or only infrequently,
use that facet of Rosegarden will reap less advantage than those who
do, although they may benefit from the included  improvements to
the [Loop Ruler](#loop_ruler), [Marker Ruler](#marker_ruler), and/or
[Chord Name Ruler](#chord_name_ruler).

Many of the features are more complex to describe than they are to
actually use in practice. It is probably better to use the following
documentation as a starting point for exploration and experimentation
rather than reading it in isolation. Users not familiar with (official)
Rosegarden may want to try the matching actions (if any) in that
version -- either before, after, or simultaneously -- if curious
about the differences.

Note: Full usage documentation for almost all new features (limited
only by Qt restrictions) is available via hover/pop-up help and/or
the context help area at the bottom of the Matrix Editor window.

<a name="percussion_matrix_editor_unification"></a>
#### Matrix/Percussion Editor unification

* The separate "Percussion Matrix Editor" has been eliminated, as the
  improved Matrix Editor correctly displays either normal or
  percussion segments without requiring the user to explicitly choose
  between the two.

* More importantly, the new version correctly displays and allows
  editing a mixture of normal and percussion segments when
  editing multiple segments simultaneously.

<a name="show_percussion_durations"></a>
* Note that Rosegarden creates percussion "notes" with automatically
  determined "durations" (time between MIDI Note On and Note Off
  events), either until the next note on the same percussion
  instrument or a whole note, whichever is shorter. Percussion note
  durations usually only matter for instruments such as Standard
  MIDI's "Whistle", "Long Guiro", etc., but for edge cases where
  viewing/editing the durations is required, the Matrix Editor's
  "View -> Notes -> Show percussion durations" checkbox can be turned
  on.

<a name="color"></a>
#### Color

* This forked version of Rosegarden uses color more extensively than
  the official one. A new Main Window "Edit -> Segment colors" menu
  allows automatic setting of some or all segment colors without
  having to manually change each one individually. Several coloring
  algorithms are available via sub-menu choices.

* Additionally, the same menu's "Display" sub-options allow a choice
  of showing segment colors in the main window in the previous
  "Selected dark" mode (in which selected segments are darkened while
  non-selected ones show their correct color) vs the new, more
  logical, "Selected bright" mode (selected segments visually stand
  out in their full/normal/bright colors while non-selected ones are
  shown with subdued hatched color lines to indicate their current
  lower priority).

<a name="additional_main_window_features"></a>
#### Additional Main Window features

* A new "Tracks -> Delete Empty Tracks" menu is useful when loading
  a MIDI file for inspection/analysis. Many such files contain
  superfluous tracks (titles, etc.) which clutter the interface and
  in particular interfere with the automatic coloring of segments
  (the short supply of visually contrasting colors is "wasted" on the
  empty segments). If a track contains only segments without
  notes, this menu option deletes them, saving a time-consuming
  search through the entire composition's time range to see if any
  off-screen, late occurring notes exist.

<a name="matrix_editor"></a>
#### Matrix Editor

In addition to the Percussion Matrix Editor unification described
[above](#percussion_matrix_editor_unification), most of the fork's
features pertain to the Matrix Editor.

##### Note display

There are multiple new options/modes for displaying notes.

* The "View -> Notes -> Color" options allow showing notes with color
  based on velocity (the only option in official Rosegarden), or in
  the the note's segment's color (as set in the main window) . This
  latter option greatly aids editing multiple segments/instruments
  simultaneously. There is also a somewhat less useful
  "Segment+velocity" mode which displays the segment color but with
  lightness based on velocity, and an "Only active" checkbox for
  displaying notes in non-active segments without color (potentially
  useful for directing special attention to the segment being edited,
  at the cost of losing information regarding what voices the other
  notes belong to).

* The "Show percussion durations" menu was described
  [above](#show_percussion_durations).

* The "View -> Notes -> Note names" menu contains a large selection
  of note naming options. Some are are dependent on the composition's
  key at the time of the note, such as "In-key scale degree" (1-7
  plus sharps/flats), "Movable-do solfege" (do- or la-based), and
  "In-key integers" (12-tone).  Others are key-independent: "Concert
  pitch" (standard letter names), "Fixed-do solfege", and "Integer"
  (12-tone) (all with MIDI octave number), and raw MIDI note numbers.

* If the Chord Name Ruler is active and a note belongs to a chord, it
  can also be labeled with an optional suffix describing the note's
  function within the chord (enabling the suffix also automatically
  activates the ruler if not currently checked "on" in "View ->
  Rulers"). The chord note functions can be labeled with either chord
  degrees (1 to 7 plus possible "#" or "b"), or 12-tone integers.

* There are several more label variation options, including whether
  accidentals in the ambiguous key of C major should be shown as
  sharps vs flats. Additionally, observant users may notice subtle and
  not-so-subtle changes and improvements such as the scaling of note
  label text at extreme horizontal scales, and in the graphical
  indication of selected notes.


##### "Tools" and editing workflow

* The Matrix Editor's "tools" -- MultiTool (formerly Select), Draw,
  Erase, Move, Resize, and Velocity -- have been extensively
  redesigned and enhanced.

* Complete/comprehensive/accurate help text is displayed at the
  bottom of the Matrix Editor window for all tools, describing
  options available in the current context (e.g. whether the mouse
  pointer is on a note or not, and the state of the mouse buttons and
  current in-progress editing task). Additionally, the onscreen mouse
  cursor changes to indicate the current editing mode/context.

* Active segment selection: As per mainstream Rosegarden, only notes
  in the current active segment can be selected, modified, created,
  or erased. The active segment selector roller widget is still
  available in the lower left hand corner of the Matrix Editor
  window, but the active segment may also be changed via the menu
  displayed when right-clicking if not over an existing note, or
  even more easily by clicking any note in the segment to be
  activated. (Exception: The control key must be held down when doing
  so in the Draw tool in order to differentiate segment activation
  from note creation.) Note that this is an example of when segment
  [colors](#color) (enabled via "View -> Notes -> Color -> Segment"
  or "Segment+velocity") greatly aid ease of use.

* "Alternate" tools: Each tool has an associated "alternate tool"
  which can be used for a single operation, or toggled on until
  released back to the original. The expectation is that experienced
  users will do almost all editing either in the MultiTool or the
  Draw tool, each of which is an alternate for the other. All editing
  tasks can be achieved with the combination of these two tools, with
  the choice between which one to use as the "main" tool and which
  the alternate driven by whether currently/primarily modifying
  existing notes vs creating new ones.

* Regardless the above, the special purpose and now somewhat
  superfluous Erase, Move, Resize, and Velocity tools are still
  available for ease of use (or simply user preference) when doing
  editing specifically tailored to their particular capabilities.
  Example: The MultiTool can move, resize, or adjust the velocity of
  notes depending on what part of the note is initially clicked, but
  when "small" notes (either due to duration, horizontal zoom scaling
  of the Matrix Editor, or both) are being edited it is sometimes
  hard to click precisely enough to select the desired action. In
  that situation and others (e.g. using the Erase Tool's single-click
  erase instead of the double-click available in all tools when
  erasing many notes) the single-purpose individual tools are easier
  to use.

<a name="features_in_all_tools"></a>
The following capabilites are available in any/all tools:

* Selecting note(s):
    * Click a note to select (replaces any previous selection).
    * Shift-click note to add to selection, or remove if already in
      selection.
    * Click-drag to select multiple notes (replaces any previous selection).
    * Shift-click-drag to add multiple notes to selection, or remove
      from selection if all notes in drag area are already in
      selection. (Exception: Must hold Control key in Draw tool to
      differentiate from normal note drawing.)
    * Double-click to delete note(s)
        * Delete clicked note if no existing selection or note not in
	  selection. Also deselects any existing selected notes.
        * Delete all notes in selection if clicked note is in selection.
    * Right-click
        * If not over note brings up menu with options to switch to
	  another tool), plus undo/redo, plus change active segment.
        * If over note brings up Event Properties dialog. Shift-click
	  for Intrinsics dialog.
    * Click non-active segment note to change active segment.
      Exception: Must hold Control key in Draw tool to differentiate
      from normal note drawing.

* Alternate tools:
    * Each tool has an associated "alternate tool" which can be
      switched to temporarily or semi-permanently.
        * Middle-click performs a single right-click action in
	  alternate tool.
        * CapsLock toggles alternate tool on and off.
    * Alternate tools:
        * MultiTool --> Draw
        * Draw --> MultiTool
        * Erase --> Draw
        * Move --> Resize
        * Resize --> Move
        * Velocity --> Resize

<a name="individual_tools"></a>
Individual tools:
* MultiTool (formerly "Select and Edit"):
    * Move, resize, or change velocity of note depending on whether
      mouse is in middle 70%, right 15%, or left 15% of note,
      respectively.
    * If percussion segment (and "View -> Notes -> Show percussion
      durations" is not checked "on"), move or change velocity
      depending on whether mouse is in lower 80% or upper 20% of
      note, respectively.
    * Hold Control to duplicate note(s) before moving.
    * Hold Shift while moving (normal or with Control/duplicate) to
      disable snap to matrix grid.
* Draw tool:
    * Click to create note, drag to resize.
    * Hold Shift to disable snap to matrix grid.
    * Control-click on non-active segment note to disable note
      drawing and perform normal non-draw-tool change of active
      segment.
    * Control-click-drag when not over note to disable note drawing
      and perform normal non-draw-tool click-drag select.
* Erase tool:
    * Click to delete single note if not in selection.
    * Delete selection if clicked note in selection.
    * Tool largely superfluous given double-click delete available in
      all tools, but convenient (single- vs double-click) if
      interactively deleting large numbers of notes not easily
      click-drag-selectable.
* Move tool:
    * Same as MultiTool click in middle of note.
    * Largely superfluous given the above, but convenient for moving
      "small" (short duration) notes without necessitating large
      horizontal zoom size.
* Resize tool:
    * Same as MultiTool click in rightmost 15% of note.
    * Similarly superfluous as per comments on Move tool, above.
    * Will only resize last of tied notes, and only resize those to
      right (later music time).
* Velocity tool:
    * Same as MultiTool click in leftmost 15% of note.
    * Similarly superfluous as per comments on Move tool, above.

<a name="other_new_features"></a>
Other Matrix Editor changes and improvements

* New highlight ("stripe") modes: In-key vs accidental, and
  alternating light/dark (for percussion segments).

* Visual distinction between vertical lines indicating measures,
  beats, and inter-beat grid timing.

* Percussion "notes" are created on the nearest vertical
  measure/beat/grid line instead of the nearest earlier time (matches
  semantics that drum hits occur on the beat, not between beats).

* The vertical piano keyboard at the left of the Matrix Editor window
  more accurately aligns with the note grid.

The following changes/improvements to the Rosegarden "rulers": Loop,
Marker, and Chord Name -- are available (with minor differences) in
the matrix and notation editors, and the main window.

 <a name="loop_ruler"></a>
#### Loop Ruler

The loop ruler has been extended/enhanced in comparison to both the
longstanding implementation in official Rosegarden 21.12 and prior,
and also the as-of-this-writing upcoming changes in 22.12.

The changes largely center around separating loop range definition
from looped playback, and are partially motivated by the fact that
the loop range is additionally used for purposes other than looping
(e.g. selecting notes by time range in the Matrix Editor).

* Looped playback (or or off) is controlled independently by the
  "Loop" toggle buttons (keyboard "@" hotkey/accelerator) in the
  Main, Transport, Matrix, and Notation Windows. (The buttons are
  synchronized together.)

* If a loop range is defined and is active (see immediately below),
  looped playback is confined within the range. If not, looped
  playback applies to the combined min/max time ranges of the
  composition's segments.

* The loop range is defined via mouse right-click-drag in the loop
  ruler. It can be adjusted via right-click-drag close to its
  beginning or end points

* The range is active immediately after being defined. When active,
  its time span is displayed in green. Times before the range are
  displayed in red crosshatch, and times after in blue crosshatch.
  This aids in knowing whether an offscreen loop range is before or
  after the currently zoomed/panned view.

* The loop range is toggled active/displayed vs inactive/invisible
  via mouse right-click *without* drag. If an attempt is made to
  toggle it active without first defining the range a warning dialog
  appears. (Note the dialog has an option to disable repeated
  warnings.)

* The loop range is automatically clipped to the min/max range of the
  compostion's segments, both during range definition and afterwards
  if segments are modified or removed.

* Playback cursor movement (click, or click-drag) and playback
  stop/start (double-click) remains unchanged or slightly improved
  (bug fixes).

Loop range playback with the loop range defined and active behaves as follows:

* Starting playback (play button, double click, spacebar
  hotkey/accelerator, etc) starts at the current cursor time if
  within the loop range.

* If current time cursor is before the loop range start time, or
  at or after its end time, playback jumps to start time before
  beginning.

* Stopping playback (play button, double click, etc) leaves the
  time cursor at its current position.

* If the looping button is toggled "on", playback continues to
  loop indefinitely. If not, playback stops at the end of the
  loop range. The latter allows repeated single playback of the
  loop range under manual control (once per playback start).

 <a name="marker_ruler"></a>
#### Marker Ruler

Several bugs in the Marker Ruler have been fixed, existing
capabilities improved, and new features added.

* If markers have been defined (newly created or loaded along with
  composition from file), they are listed in time-sorted order in the
  Marker Ruler's right-click menu and can thus be accessed (jumped
  to) with a single mouse right-click and menu choice. This is as
  compared to official Rosegarden's requirement to double-click to
  bring up the "Manage Markers" dialog box, moving the mouse into it,
  and then clicking the marker there (all of which is still
  supported). Of course clicking on a marker that is currently
  visible onscreen (pan/zoom of window) is also still supported but
  is less frequently useful (one could simply move the playback
  cursor via the Loop Ruler, but neither method is applicable in the
  canonical case when the desired marker is not currently visible).

* New markers are created with a default name of "M:B" or "M:B+NT"
  where "M" is measure, "B" is beat, and "NT" is musical note time
  past the beat if not exactly on it. Compare to official
  Rosegarden's "new marker" default name, which is indistinguishable
  when used for multiple markers. 

* Because the intended use of markers is to allow quick access to
  meaningful places in composition ("2nd chorus", "movement #3,
  Allegro", "horn section enters here"), when adding a new marker the
  marker editor is immediately popped up to change the
  above-described default time-based name. Keyboard focus is in the
  name field to allow immediate editing without an additional mouse
  click. If the default "M:B+NT" name is acceptable, clicking "OK",
  "Cancel", or pressing the "enter" key pops editor down without
  change, so this feature requires only one extra click or keypress
  when creating a default-named marker. Regardless, if this workflow
  is not desired (e.g. a user who frequently accepts the default
  time-based name without change, possibly editing it later via
  double-click) the feature can be disabled by unchecking the "Edit
  immediately after insert" box in the menu. 

* Several bug fixes, including:
    * The Main Window's "Composition -> Jump to Next Marker" (and
      "Previous") used order of creation instead of time order for
      newly created (vs loaded from file) markers.
    * "Shift-click to set a range between markers" was similarly
      broken (incorrect order, not time-sorted).

* Markers have easily visible down-pointing arrow graphic to left of
  text indicating exact time position of marker instead of a
  difficult-to-see thin vertical line.


 <a name="chord_name_ruler"></a>
#### Chord Name Ruler

The Chord Name Ruler has been completely rewritten and extended,
along with its underlying chord analysis engine. New features and
changes include:

* Greatly increased number/types of chords: Extended chords through
  13ths, augmented/suspended/sixth/altered chords, omit note chords,
  etc.

* Right-click menu action to insert key change (at playback position)
  in matrix and notation editors. (Was previously only possible via
  unrelated "Segment" menu or using the Event List Editor.)

* Two different chord analysis modes: When notes on/off, or all notes
  in a specified time range (supports analyzing arpeggios as chords).

* Extensive user menu/preferences for chord naming (concert pitch
  letters, roman numeral scale degrees), ("m"/"min", "aug"/"+",
  "maj7"/triangle7), (minor chord roman numerals "IIm" vs "ii"), etc.

* Optional "slash chord" analysis/naming with several
  menu/preferences options (non-chord bass notes, chord or key/pitch
  relative bass note names, simplified chord plus added bass vs
  extended chord).

* Simplistic/primitive bass note disambiguation of enharmonic chords
  (C6 vs Am7, etc).

* Dialog to choose of subset of segments in matrix or notation editor
  to use for notes in chord analysis. Allows limiting excessive
  reporting of extended chords by e.g. disabling melody instrument
  segments.

* Usage suggestion: Add a segment to control chord analyses (add
  missing notes, add bass notes for enharmonic disambiguation, etc.)
  The segment can be set "unvoiced" by creating its notes with zero
  velocity (or later adjusting them) and/or muting segment's track.

* Chord name ruler action (mouse double-click) to delete key changes
  (previously only possible via the Event List Editor).

* "No chord" ("n/c") labels.

* New option in the Notation Editor's chord name ruler to copy chord
  name ruler chords to text annotations (automatic lead sheet
  generation). Does not overwrite existing text, so individual chords
  can be hand-edited as desired. Or multiple/all chords can be
  drag-selected and deleted, chord analysis options changed, and the
  new chords copied to text in the missing positions.

* Choice of flats or sharps in key of C major.

* Chord label to right of tick mark (formerly only for key changes).
  Rationale: Chords start at time and continue for their note's
  durations vs sounding only for one instant.

* Chord and key names are always displayed at correct time in ruler
  (previously were "pushed" to right if not enough space). Rationale:
  Better to omit information than present false information.

*  Visual distinction between time tick marks for chords vs key changes.

<a name="general_changes_and_improvements"></a>
#### General changes and improvements

* Both playback and time cursor motion ("Rewind to Beginning", "Fast
  Forward to End") stop and move, respectively, to the combined
  min/max limits of the composition's segments instead of to the
  compostion's maximum measure number (default 100).

* With default settings for "Track Parameters -> Thru Routing" and
  "Instrument Parameters -> Receive external", incoming MIDI notes
  always play using the currently selected track's instrument in
  whichever window (main, matrix, or notation) currently has mouse
  focus.

* New Preferences -> General -> Behavior option to save .rg files
  uncompressed.  Useful for RCS/CVS version tracking of compositions.
  Note that using `git` provides few benefits because .rg files are
  both standalone and normally already compressed, so git's
  sophisticated multi-file versioning isn't applicable, and the
  RCS/CVS "diff" approach is more effective than attempting to
  re-compress already compressed files.

* True unicode sharp and flat symbols instead of ASCII "#" and "b".

* The playback time cursor vertical line's visibility has been
  increased in the main window and the matrix editor.

* Codebase cleanup, simplification, and rationalization. First steps
  at adding finer-grained internal messaging (Qt signals, Composition
  and Segment Observers), and partial removal of excessive/gratuitous
  uses of the Qt signal/slot mechanism.



<br> <a name="further_information"></a>
Further information
-------------------

For further information see [README-official.md](README-official.md)
and/or the official Rosegarden links listed
[above](#acknowledgment_and_motivation).
