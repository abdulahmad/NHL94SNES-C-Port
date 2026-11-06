# The routines

Every function in `game/`, by folder and file: the C name, the original routine it is (the
disassembly's label of the same name and its cartridge address), and what it does. Generated from
the code's own comments by `tools/routines.py`. A helper is a piece split out of an original
routine, a part a routine shares with others, or a small function of the C's own.

880 functions: 702 original routines, 178 helpers.

## Play: the game on the ice (the game loop, skating, the AI, checks, the puck, goalies, penalties, the shootout)

### `game/play/collide.c` (47)

| Function | Original | What it does |
|---|---|---|
| `getpde` | `$9FDCB2` | The energy of player o (getpde leaves the team, nonzero for the visitors, in a2 and the player number * 2 in d1: setpde's arguments) |
| `setpde` | `$9FDCD3` | Set the energy of player num2 / 2 of team (nonzero: the visitors) to energy, 0 when negative |
| `Setplass` | `$9FCED0` | Player o's first assignment, from his position (none off the ice). |
| `GetPeriodTimeRemaining` | `$9F9B08` | The period (gsp, in bits 14-15) and the time into it, for the goal and penalty logs. |
| `makepde` | `$9FDCEB` | Rating a scaled by the energy of player o (a0) (product / $1000); the carry is left clear. |
| `checkagr` | `$9EC124` | Player o's penalty roll: random below (20 - aggress) * 16, doubled on a pad, halved within 40 of the puck. |
| `TryAddPlayerToList` | `$9FD6D8` | Roster player d0 of team a2 into slot d4 of the list at a4 if he is available (on the ice, on the bench or not yet there, and not in the list): returns him (Z clear), or 0 (*d1 = 0). |
| `rb` | (a helper) | (ResetBench for team a0 = 0 / 2 with box count unit d1: every player counted, and taken off again when on the ice or the bench (who goes to the bench) or injured with no time left) |
| `ResetBench` | `$9FCDAC` | Empty the penalty boxes and put every player on his own bench. |
| `deflect` | `$9EF722` | The puck is deflected: loose, a random velocity (x, y within +-$1000, height up to $FFF), spun. |
| `sp_adj` | (a helper) | (setplayer's .8: rating a plus the player's hot / cold byte for stat k (/ 4), 0-15, in 8 bits) |
| `setplayer` | `$9FCEF6` | Bring roster player d3 onto the ice as object o: off the injured list, out of the box (assignment $0B) or off the bench ($0A), on the ice; his 12 ratings from his roster record with the power play, penalty kill, home / visitor and late third period bonuses (none for a goalie) and his hot cold bytes. |
| `forcepldata` | `$9FCE1D` | At a face off: team a2's six players take their new positions (newpos, a byte) straight away, with their assignments (and a centre the face off one, $12), and their new roster players (newpnum) on the ice. |
| `setInjuryType` | `$9EC2D0` | Player a2 is injured: pf2inj, the crowd louder (held 60 frames) and groaning (stream 4), the screen locked on him, his roster slot out of the game (1 in 8 rosterNone, TempPlOffset bit $4000), who it is in TempPlOffset. |
| `wallcoll` | `$9E8F42` | Object o bounces off a wall of angle cos / sin (kept in Wallcos / Wallsin): its velocity split into v1n (into the wall) and v1t (along it), each cut, rotated back into a3's velocity. |
| `wallcollb_out` | `$9E8F05` | The puck is out of play: the screen locked, the puck out of collisions (pfnc = $40), flipped at the bottom end, its shadow gone; with the clock running the last toucher's shootout result (ShootoutWonBy 6); a penalty shot over. |
| `wallcollb` | `$9E8E91` | Object o hits a wall (cos / sin, a3 for the bounce): a high puck, or a fast one rising into the net's top behind the goal, goes over the glass (the shadow's glass animation, stream 6, the crowd) and out of play; anything else bounces. |
| `clamp_ff` | (a helper) | (checkgoalp: a cosine / sine of the bounce off the goal kept to -$FF .. |
| `checkgoalp` | `$9E92F8` | Player o (a3 from the first test on) runs into goal a2 at d2 / d3 (wcradius around it): close to its back he bounces off it (wallcoll, the angle from the goal's centre). |
| `at_least_800` | (a helper) | (checkgoal: the puck's speed on one axis made at least $800 either way) |
| `move_hi` | (a helper) | (a coordinate's move this frame from the old position, in 1/256: (pos:frac - old) >> 8) |
| `checkgoal` | `$9E90AE` | Object o meets goal a2 at d2 / d3 (a3 as for wallcoll): players by checkgoalp. |
| `clamp_ff2` | (a helper) | (checkwallcoll: DivX8S's quotient kept in -$FF..$FF, the lower limit first) |
| `checkwallcoll` | `$9E8D3E` | Object o at d2 / d3 against the rink: in an end zone's corner circle (64 in from the boards, by its radius) a bounce off the curve (wallcollb, the normal from the circle's centre); between the corners at the ends the goals (checkgoal); then, when nothing hit, the straight boards. |
| `puckglue` | `$9E9A49` | Player a2 takes the puck (a new carrier: the puck sound 3, a penalty shot over): the other team's touches cleared (flag 4), a face off won counted (the crowd), a completed pass counted; a goalie catching it makes a save (ChkShotStat) and holds it (temp5 140, 5 after a dive); the pass receiver's pad goes to him, or the next pad of his team (PuckCarrierPad). |
| `SetPlList` | `$9FD55D` | The list of the players team a2 wants on the ice now (PlList: six roster numbers, then the six positions they take): from its current line in priolist's order for the men it has (a penalty shot: the shooter and the goalie; no goalie when he is pulled), each one not available replaced by the first available of his position's substitutes (sublist; $80: anyone, from the last roster player down). |
| `setpersonel` | `$9FD407` | Put team a2's wanted players (SetPlList) on the ice: the newpos / newpnum requests of its six objects cleared, then a wanted player already on the ice keeps his object, and each other one takes a free object (the last free one found, one in play preferred). |
| `excite` | (a helper) | Raise the crowd's excitement CwdExciteLvl by n (a hit, a fall), unless it would go negative |
| `FallDown` | `$9E9B52` | Player y (a2) falls or stumbles from a hit by x (a3 the hitter after it; the team check counted for x): near the boards he goes into them (by where he is and where the hit came from, put against the boards or the corner's curve), by the bench over it; otherwise he falls back (hit from the front), forward or flips (a strong checker); a skater with the puck stick-handling well and no lockout only stumbles. |
| `check_int` | (a helper) | (checkint's .ci: goalie a2 against player x (a3): with the clock running and x's impact past 26 (past 3 when a3 has the puck), x falls (FallDown by the goalie); a hard hit on the goalie (past 31) may be interference on x (by his aggression), unless he is out of his crease) |
| `checkint` | `$9EBA4F` | A goalie and another player collide (x and a2 / a3 each way: check_int): the skater falls, and interference may be called. |
| `penshot_foul` | (a helper) | (a foul on player a2 when he is the carrier on a breakaway and no shooter is set: a penalty shot (on x) for him; nonzero then) |
| `Bcheck` | `$9EBC24` | Player a2, sweep checking, body checks x (a3): unless x is locked or a goalie (or, with penalties off, a human checker loses a roll on checking against x's legs), when x is in front of him a3 falls (FallDown by a2), then a penalty shot or (by aggression) tripping; collflag set. |
| `holdcheck` | `$9EA2F1` | Player a2 holds or hooks x (a3), who is in front of him: a2 turns to him, both take half their summed velocity, x flails (pfalock) and a2 holds (SPAHold2, or hooks), each the other's impactp; the carrier's shot is off; then a penalty shot or (by aggression) holding / hooking; collflag set. |
| `check_check` | (a helper) | (checkcheck's .cc: x checking a2: a2 holding or hooking is holdcheck, sweep checking Bcheck; otherwise x bursting into a2 plays a check by the direction (from the table .4, a shoulder or hip check, or at random the next one), and a skater a2 hit hard enough (d4, the impact, past $20) goes down on a roll against x's checking, with charging, roughing or a penalty shot by x's aggression; returns d4 as left) |
| `checkcheck` | `$9E9FF9` | Two players in contact: what happens each way (check_check of a3 on a2, then of a2 on a3). |
| `pl_pair` | (a helper) | (checkplcoll's .t3: players x (a3) and y touch (within $10) while closing in: any hold between them or their partners is broken; opponents take the impact (the closing speed / 16, at least 5, half again in a sweep check; the collision sound when hard with the carrier), then checkint and checkcheck; unless that set collflag, they bounce apart by their weights (+$8C), collflag set) |
| `pl_near` | (a helper) | (checkplcoll: object z of OOlist can touch x, at new x d2: in play, within 15 in x, no flag 15 pfnc, a player) |
| `checkplcoll` | `$9E9484` | Player x (a3) at his new place d2 / d3 against the players near him in y (OOlist, up then down, within $0E): pl_pair. |
| `checkcoll` | `$9E8C35` | Object x moves to d2 / d3 (swapped on the horizontal screen): the walls (at its hot spot, and a player's stick end too when the body hits none) and the players (checkplcoll), unless it skips collisions; after a player collision it goes back to its old place (but into the boards); otherwise it moves along OOlist (kept in y order) and its Ylist y is the new one. |
| `checkpuckcoll` | `$9E9788` | The puck (x, low: under $10) against the objects near it in y (OOlist, up then down, within $28): checkpuckcoll_ccx for each (a0 as the caller left it, for puckstick). |
| `puckbody` | `$9EEF7F` | The puck x hits player a2's body (d0 the squared distance from ccx): unless it is low and off by $10's remainder, he touches it with the puck sound 8 (the original compares a2touchpuck's A, never $E or more, where it meant the puck's height), the puck takes his offset (or his hot spot) as its velocity's high bytes and flips; a fast high puck (speed squared $0900 0000 or more, $C up) knocks him down (FallDown by a3), a slower one is caught. |
| `puckstick` | `$9EF5DA` | The puck x hits player a2's stick (d0 the squared distance, a0 the carrier as the original has it: what a2touchpuck left there, the carrier's team offset; read as the original leaves it, as an earlier puckstick of the same checkpuckcoll may have changed it). |
| `puckgoalie` | `$9EF042` | The puck x hits goalie a2 (d0 / d1 its offset from ccx): the save rating of the side it comes at (low: glove or stick, by his facing; high: the other two), unless he is diving and it is high; then the shot counts, he touches it (the sound, the crowd for a hard one); a carrier loses it. |
| `SetupPenaltyShot` | `$9FAFBE` | Set up a penalty shot: the puck at centre ice and still; each skater of the shooting team (BA_Team) takes assignment $10 eight times over (puckc him meanwhile); with manual goalies the other team's pad takes its goalie; the shooting team's touches cleared, the crowd excited; the shooter (BA_Sktr_SCnum) has the puck (and the pad pspad); PenShotActive, penshotlive, the time limit pstimer 1000; the penalty buffer cleared. |
| `Goal` | `$9EF319` | The puck is in the net (o, the puck): the score and team stats, the siren and the crowd, the score summary and the players' goals and assists, the scoreboard, the scorers celebrate, the puck pops up at the goal line and the goal whistle. |
| `checkpuckcoll_ccx` | `$9E97DD` | The puck x against player y (a2): unless he carries it, is past the skaters, off the ice or not colliding. |

### `game/play/hockey.c` (46)

| Function | Original | What it does |
|---|---|---|
| `PauseSubmenuDone` | `$9FE2AE` | The end of a pause submenu: the whole text map redrawn, A = 1 |
| `restoreteams` | `$9FC702` | Put both teams' players on the bench (tmap 6) |
| `updatecrowdf` | `$9B9F7A` | Every game loop: the crowd's hold counts down (at most 60), then its level (at most $480) falls by impactstep, to 0 |
| `seta2` | `$9FC76C` | The team of the controller that paused (a2: 0 home, 2 visitors); a negative pausepad is minus the side whose first controller counts |
| `ChoosePauseMenu` | `$9FE4BF` | The longer pause menu when this side still has a timeout (clock stopped, not the intermission), else the standard one; the first item shown kept inside it |
| `RestoreBenchEnergy` | `$9FDDCD` | With line changes on, the benched players of both teams get 10 energy back, up to $1000 |
| `start_hdma` | (a helper) | The window and COLDATA HDMA (channels 2 and 3): window 1 from left to right, the tables in bank $9D |
| `ResetClock` | `$9FC683` | Set the period length (a shootout $19, overtime of a regular season game 300) and stop the clock |
| `clear_braces` | (a helper) | Erase the braces at stats page rows top and bottom (column x) |
| `PmenuAppend` | `$9FEB90` | Append string a4's characters from y + 1 to the pause item text in mesarea (a word at a time, its length byte counted one up each) |
| `pmenu_value` | (a helper) | Append a pause item's value (the goalie or defense control setting of the side that paused) when item d0 is the one PmenuValueItems names for this menu |
| `DrawExitPrompt` | `$9FF063` | Draw the exit question (game or shootout) and the No / Yes lines, the one at curitem highlighted |
| `pmenu_goalie` | (a helper) | SetupPauseScreen's goalie item: when item d0 is the menu's goalie item, the side's goalie's last name (or None) |
| `SetupPauseScreen` | `$9FEA24` | Draw the pause menu: five items of the current list from psfirst, the selected one (psitem) highlighted, each with its value, centred on column $11 |
| `clockcont_0` | `$9F8B79` | The end of a period: the puck's whistle assignment and the horn; from the third period on, unless tied, the skaters of the team ahead celebrate (the first of them with assignment 9 and every pad flag cleared after a won Stanley Cup final in a playoff mode), the game is over: the crowd, the end of game message (and the cup's). |
| `clockcont` | `$9FA626` | Once a game loop: when the clock has run out, freeze the view and end the period |
| `CheckInjury` | `$9EC2C7` | Once a second: the count down to the next injury, its box at 0 |
| `periodicevents` | `$9F8646` | Every game loop: the penalties, the crowd, the sound and the clock; once a second of play (not on the horizontal rink: lldisp counts 24 frames) the goalies, the bench energy, the injuries, the power play |
| `DoGameFrame` | `$9F8625` | One frame of game logic |
| `PulsePauseSide` | `$9FE75E` | Pulse the colour of the pausing pad's side (colourbuf+$1DE or +$1FE), the other side white; both white in a demo or with no side |
| `mp5_box` | (a helper) | The MultiPlayer 5 warning box for warning bits a (1 the multitap, 2 the Super Scope, 4 the mouse) |
| `ShowMP5Warning` | `$9FF1D1` | Show the MultiPlayer 5 warning while there is one, then the pause menu again |
| `PulseUntilRelease` | `$9FE9FD` | While the pause pad holds A or B: pulse its side's colour and send it to CGRAM each frame |
| `frame_steps` | (a helper) | The frame step sizes from the frames elapsed d7: timestep, impactstep (x2), velstep (x16), gravstep (x96) |
| `AskExitGame` | `$9FF00B` | Ask "exit game?": up / down choose (curitem 0 or 1), A / B take it, Start cancels (0); returns it |
| `demo_read` | (a helper) | In the demo: Start pauses, any other button ends it (Opening2); in mode 4 with pads on teams, Start on any of them pauses from that pad |
| `demoread` | `$9F967B` | The game loop's pad check after each frame: in the demo a button pauses or ends it (demo_read) |
| `SelectPauseController` | `$9FE618` | The pause controller: the pad that has it (a negative pausepad: the first pad of that side to press a button takes it, the menu rebuilt), its buttons with repeat in *d1 / *d3; it is given up after a while idle; in the intermission's demo the pause times out (Start). |
| `PauseResume` | `$9FE733` | The pause menu handlers (the item chosen, with the pad's new presses d1); A: 0 leaves the menu |
| `PauseDefControl` | `$9FE418` | Defense control: A / B / left / right turn it on or off for the pause controller's team |
| `PauseExitGame` | `$9FE741` | Exit game: A / B ask to be sure (AskExitGame), then the game is abandoned for the option screens (Opening2) |
| `PauseReplay` | `$9FE272` | Instant replay: A / B fade out to the replay (ReplayMode, the music noted in replaymusic) and back, the frame count kept |
| `pause_handler` | (a helper) | The handler at addr (jmlvector) |
| `PauseMenuPoll` | `$9FE19E` | The pause menu: the items shown, the pause controller's buttons (Start resumes), up / down move the item (five shown from psfirst), another button runs it (its handler from PauseMenuHandlers through the RAM trampoline, by menu list) |
| `Pausemode_menu` | `$9FE11D` | Pick the pause menu (a shootout: 4, or 3 before the game; the overtime period: 1; before the game: 2; else ChoosePauseMenu's), a frame of sprites, both sides' colours white, then the menu |
| `Pausemode` | `$9F937B` | Pause mode: the text map saved, the sound stopped, the scoreboard screen with the pause menu until it is left, then the text map and the screen back, the pause sounds, the frame clock restarted |
| `Gameloop` | `$9F8511` | The game loop, a pass per frame elapsed: the frames per second, a team song now and then in the third period without music, the step sizes, the pads, the clock (or the penalty shot timer: its end), the crowd's cheers, a frame of the game, the demo's pads, the pause (forced by a MultiPlayer 5 warning). |
| `summary_page` | (a helper) | A summary list's rows shown (from listtop, listrows of them, 3 text rows each) to VRAM y, or the none message when the list has `empty` entries |
| `pause_screen` | (a helper) | The pause menu's screens: A / B show one, then the rink comes back (A = 1); other buttons go on polling |
| `PauseEditLines` | `$9FE305` | Edit lines from the pause menu: A / B open the line editor (LineEditor), then the rink is reloaded |
| `PauseShootoutSetup` | `$9FE2CF` | Shootout setup from the pause menu: A / B open the shooter order (ShootoutShooters), then the rink is reloaded |
| `start_period` | (a helper) | Start a period: the teams' energy, the rink and the clock, the pregame pause (owned by the first pad on a team), the faceoff, the replay buffer, the frame counters and the sprites; then the game loop (in the shootout the caller runs it) |
| `StartPer` | `$9F8433` | Start the next period (start_period); outside the shootout the stack is reset first, since the game loop never returns |
| `StartGame_go` | `$9F833C` | A new game from its start: game RAM cleared, the options in force, the teams, scores and lines, the pads' teams and sides, then the first period (StartPer; in the shootout the caller starts it) |
| `StartGame_scout` | `$9F8334` | A new game from the hot / cold table and the scouting report on |
| `StartGame` | `$9F8330` | A new game: the playoff screen, the scouting report, then the first period |

### `game/play/logic.c` (160)

| Function | Original | What it does |
|---|---|---|
| `SetSPAFromA` | `$9EA3F1` | Start animation anim on object o (the same one keeps going). |
| `puckunflip` | `$9EDE81` | The puck slides and its spin restarts: during the spin frames its face turns around. |
| `AssignPadPlayer` | `$9FCB68` | Pad d4 takes control of player d0: its old player is released (pfna set, his pad bit cleared, playerpad -1, a goalie's goalieflags cleared), the new one marked (pflags bit 3, playerpad). |
| `StartPenaltyShot` | `$9EC72D` | A penalty shot for shooter BA_Sktr_SCnum, unless one is pending: his team (BA_Team, its tmPenShots), one man each on the ice, the shooter's roster number; the faceoff spot saved and cleared. |
| `GoaliePuckVector` | `$9EDC44` | d0 / d1 = the vector from goalie o to the puck; when one of them is behind the goal line ($108) and the other in front, d1 = +-1 so he faces out. |
| `CalcPuckDistances` | `$9CA58C` | For each of the 16 objects (curobj each in turn): puckdist2 (about (dx^2 + dy^2) / 16 by the quarters' low bytes), puckdist (ApproxLength) and puckdir (vtoa) to the puck. |
| `DumpInTarget` | `$9EECF6` | assdumpin's target: dumpinplayer = o, dumpintarget and passdir from where he is. |
| `CheckBreakaway` | `$9EC64B` | IsBreakaway = 1 when the puck carrier, in the half he attacks, has no opposing skater between him and the goal. |
| `GetFrameHot` | `$9BC074` | d0 / d1 = the stick hot spot of object o's frame (its record's bytes $14 - $12 and $13 - $15), x negated when flipped, y unless flipped vertically, turned in horizontal mode. |
| `AssignPadIfFree` | `$9FCB5C` | AssignPadPlayer, when player d0 has no pad yet. |
| `GlideBack` | `$9E8000` | Glide backwards, unless player o's animation is locked. |
| `PuckCarrierPad` | `$9FCB16` | When player d0 has the puck and no pad: the next of his team's pads in turn (puckpadnext, from pad 4 down), if it has any, takes him. |
| `PadTakePosition` | `$9FCD69` | Pad d4 takes the player of its team in position d1, if he has no pad and is available (pflags $1000 clear); padinputdone counts it. |
| `PadTakeLeftDefense` | `$9FCD4D` | With defence control on for player o's team: pad d4 takes the defenceman on the left of the screen (LD shooting at the top goal, else RD). |
| `PadTakeRightDefense` | `$9FCD2F` | The same for the defenceman on the right. |
| `PassPadToTeammate` | `$9FCD0A` | Pad d4 of penalized player o goes to the first teammate on the ice with no pad (the search does not stop before one is found). |
| `fp_side` | (a helper) | (FaceoffPads: a pad of the team (padside low byte side) whose side was moved off ($80xx) comes back when the team has fewer pads on its side than skaters on the ice, counted against the home team's pads (for both teams), from padsitnext on) |
| `fp_pads` | (a helper) | (FaceoffPads: each pad of the team in turn from the one after fopadfirst gets a player (findpc, run with the pad's index as the player); fopadfirst = the first) |
| `FaceoffPads` | `$9FC988` | At a faceoff: the distances to the puck, the pads moved for a penalty shot back on their sides, pads given back to a team short of them, the pad and goalie tables reset and every pad given a player again (findpc, a3 as given). |
| `dumpin_lob` | (a helper) | (assdumpin's lob: aimed at the target's spot spread at random (more for a worse passer), its speed from the distance (at least $10, a goalie $20, at most $60), lifted and spun; the carrier (g.curobj) lets go of it and swings) |
| `assdumpin` | `$9EED8D` | Assignment $14: the puck carrier o dumps the puck in: once the target is picked a computer player shoots instead toward $14 / 8 (CompShoot: returns nonzero, his caller ended too); he turns toward passdir (a goalie moves the target by the step he still has to turn) and lobs it. |
| `StartBreakaway` | `$9EC6E7` | At the blue line: CheckBreakaway; a new breakaway queues sound 5 (loudness $7000, not during a penalty shot), raises the crowd's excitement by 20 (unless that turns it negative) and counts for the carrier's team. |
| `EndPenaltyShotPlay` | `$9EC791` | End the penalty shot or shootout attempt under way: no shooter, ShootoutWonBy 10, the faceoff spot saved at its start back as the last touch. |
| `div32u_carry` | (a helper) | (Div32S's carry, which assgoalie adds in after it: by the divide's path; the long loops leave bit 0 of r0 as it was, the 8-bit one bit 8, the hardware divider the carry it came with) |
| `div32s_carry` | (a helper) | (assgoalie: the carry Div32S leaves for n / d; a negative divisor or dividend is turned positive first, and a quotient of 0 from that gives 1) |
| `ag_clamp` | (a helper) | (assgoalie's .49: a goalie between the goal lines clamps the aim's y to the crease ($FEFD-$103; x 0 with the puck); then y made relative to the goal line d3) |
| `assgoalie` | `$9ED331` | Assignment 0: the goalie. |
| `assfaceoff` | `$9FB26A` | Assignment $16: wait (unless locked in an animation) until the face off is over. |
| `assdopen` | `$9EBEDF` | Assignment $0E: sit player o in the penalty box (PBnum: $10 a home player, 1 a visitor). |
| `assfaceoffpl` | `$9FB27C` | Assignment $17: a player taking the face off. |
| `assstanley` | `$9EA69D` | Assignment 9, the Stanley Cup: skate to the side of the screen (x +-90, the scroll y) holding the cup, then face that way, locked 20 frames. |
| `check4bench` | `$9FDC4C` | When a line change wants player o off (and he caused no penalty): to the bench (assignment $0C, pf2unav) unless he is going there already or has the puck, or, for the same roster player, a new assignment from his new position. |
| `donothing` | `$9E8BD7` | A computer player stands still (doplayeracc with no direction); a human is left alone. |
| `ass_start` | (a helper) | (assdefo / asswingd / assdefd's start, as asswingo's: on offense the carrier gets asspuckc ($11) on top and the other team's puck makes it assignment def, on defense his own team's does; the clock stopped: donothing; check4bench; a human is left alone; the first time, the timers reset. |
| `ass_timer` | (a helper) | (temp1 -= d7; when it runs out, plus add, at least 0; nonzero then) |
| `assdefo` | `$9E8804` | Assignment 1: a defenseman on offense holds the blue line on his side (x +-80, at the puck x when it is on his side, halfway otherwise; y by the puck's zone, 98 when offside), avoiding the carrier. |
| `asswingd` | `$9E8968` | Assignment 3: a wing on defense covers his side (x +-100, at the puck x when it is on his side), at a y following the puck between his blue line and goal line (the limit when the carrier is a goalie or his team is offside). |
| `assdefd` | `$9E8618` | Assignment 2: a defenseman on defense: when his timer runs out, a spot (x +-65, 0 when the puck is on the other side) 40 behind the furthest attacker (led by his velocity toward the goal; at most 220 out); skates there, then looks for a check. |
| `bench_pos` | (a helper) | (assbench's .8: the new position (newpos) and its assignment; the requests cleared) |
| `assbench` | `$9FD85F` | Assignment $0C: the player skates to the bench door (x -144, y -80 home / +80 visitors) unless check4bench ends it (or he caused a penalty: out); there he glides into the wall and is locked (temp1 = 100); then his slot goes to the bench and the new player comes on (setplayer). |
| `asseben` | `$9FD9E1` | Assignment $0A: coming on from the bench: first at the door (x -136, y 14 per slot from his object number), locked in the wall animation; when it is over he faces right and skates out ($1000), the assignment over. |
| `assscore` | `$9EB0B8` | Assignment 8: celebrating a goal (bench a2 + 2 cheers when gmode $40): unless a human holds a button, player a3 (from the slow-down on: o and a3 are the same player in the game) slows to 3/4, drops the puck (a carrier test that compares his number with puckc's index), skates to the side of the screen and celebrates there now and then (the scorer pumps his arm). |
| `pad_back` | (a helper) | (asspenalty / assepen: from team a2's padsitnext on, round robin, the next pad moved off it for a penalty (high byte set) whose team is a2; it goes back to the team (padside, cont1team); the original loops for ever when there is none) |
| `pen_pads` | (a helper) | (asspenalty's .9: with 4 pads or more on team a2 and as many skaters left or fewer, a pad sits out (from padsitnext on; with 3 skaters first one comes back): its player loses it; then player o, still with a pad, passes it to a teammate) |
| `assepen` | `$9EBADA` | Assignment $0B: player o (of team a2) leaves the penalty box. |
| `asspenalty` | `$9EBCC7` | Assignment $0D: player o (team a2) skates to the penalty box. |
| `vtoa` | `$9BB445` | The direction (0-7, 8 = none) of the vector dx / dy: its signs and two slope tests (each side against twice the other, unsigned as the 65816 compares) pick it from the cartridge's table. |
| `SetSPA` | `$9EA3DD` | Start animation anim on object o (the same one keeps going). |
| `chkpk` | `$9E8272` | Z: player o is killing a penalty (no power play: never). |
| `chkpk2` | `$9E827C` | Z: player o is on the team a man short (sf2pwrtm is 0 for team 1). |
| `assexit` | `$9E8BE6` | End player o's assignment and go back to the one under it. |
| `assinsert` | `$9E8C0E` | Give player o assignment ass on top of the current one. |
| `assreplace` | `$9E8C18` | Overwrite player o's current assignment with ass. |
| `GetHot` | `$9BC002` | The hot spot of object o's sprite frame: the frame record's signed bytes at +$12 / +$13, x negated when the frame is flipped (attribute $0800), y unless it is flipped vertically ($1000); on the horizontal screen turned a quarter, (x, y) -> (y, -x). |
| `AdjustFacingDirection` | `$9EDC06` | Turn goalie o's facing a step the short way toward want, at most a step per 6 ticks. |
| `playeracc` | `$9EA719` | Player g.curobj accelerates toward direction dir, up to his top speed: a step of the direction (none into the wall he last hit), scaled by weight and leg strength and the frames elapsed (d7), added to his velocity unless his speed would pass the limit for his leg speed and energy; then, with line changes on, one call in 64 wears his energy down. |
| `noturn` | `$9EAB64` | Flip direction dir when player o skates backwards, then accelerate toward it. |
| `skatetopuckinit` | `$9EDEA7` | Where the puck will be soon: its position plus half the high byte of its velocity. |
| `puckIChk` | `$9EF751` | Icing: once armed and not yet called, the loose puck past the far goal line calls it. |
| `CompShoot` | `$9EE10B` | Computer player o shoots: the swing time temp2b is the distance to the goal line / 8, at most 20, and the assignment becomes assshoot ($13). |
| `puckshadow` | `$9EEF34` | The puck shadow (object o) follows the puck, one unit off it, while the puck frame shows. |
| `EvadePC` | `$9E8904` | A skateto callback: when the puck carrier will be within 40 of player o next frame on both axes, the direction away from him; otherwise direction d0 as it was. |
| `stopna` | `$9EA539` | Slow player o down without changing his animation: a human goalie (not carrying the puck) loses half his speed, everybody else ($70 + legstr) * timestep per axis, down to zero. |
| `avd_zones` | (a helper) | (.27 for a skater, .48 for a goalie: lim is $2C or $24) |
| `avd_skater` | (a helper) | (the skater's cases: .t2x when tmp1 = 0, else the table .26 by tmp0) |
| `avd_goalie` | (a helper) | (the goalie's cases: .t3 when tmp1 = 0, else the table .47 by tmp0) |
| `avd_mirror` | (a helper) | (a path crossing the top goal: mirrored onto the bottom one) |
| `avdgoal` | `$9EAB74` | Keep player o from skating through a goal: when the path to d0 / d1 crosses a net, aim beside it (the goalie keeps to his crease's sides). |
| `ClearOffsidesIfAllPlayers` | `$9EF8C4` | Clear team a2's offsides flag once none of its six skaters is past the blue line (y beyond $58 from his own goal). |
| `chk4shot` | `$9EDED4` | A computer player with the puck looks for a shot: in the last seconds with the clock running he shoots; with nothing special on, a threat and 1 in 4 he dumps the puck in; otherwise a random roll on his shot odds (better near the goal, worse with the goalie set, an opponent in the way doubling them) decides, when he is in front of the goal line. |
| `goalieacc` | `$9ECFDC` | Goalie o skates in direction d0 (8: none, 9: stop), turning a step at a time toward it near his goal or with the puck, or slows down. |
| `puckflip` | `$9EDE5E` | Spin the puck: its facing flipped by bit 0 of d0, its flip animation started. |
| `check4check` | `$9EA164` | A chance to check: on 2 random rolls against (20 - checking) * 2 (with penalties on), an opponent within 34 straight ahead of player o, not already holding / locked, is hooked or held (Acheck) or burst into (burst, by player a3), by where o is and 7 in 8 of the random bits. |
| `findpc` | `$9FCBB6` | Choose the player for pad d4 (X = o, a3 the player again after the search): when o is a free skater close to the puck, or the pad's best skater (distance to the puck, plus a penalty for the puck's direction against the one preferred from o's goal, the two XORed) has it already, o starts a sweep check unless locked; else the best one takes the pad. |
| `dostop` | `$9EA4F7` | Player o stops: a fast one (over $1000 on an axis) glides (backwards) or plays the stop (the puck carrier with the skate stop sound $11), then slows down (stopna). |
| `noturn0` | `$9EAAB3` | Player o's wanted direction needs no turn (d0 = wanted - facing, * 2; d2 the direction to skate): facing it he skates (backwards, stumbling injured, with the puck), otherwise he stops when moving within 90 degrees of it, else turns a step and glides. |
| `doplayeracc` | `$9EA8A9` | Player o skates toward direction d0 (low nibble: 0-7, 8 none, 9 stop): goalies by goalieacc; skaters decide on skating backwards (in their own half, the puck ahead, or staying so), then turn toward it at a rate falling with speed, or need no turn (noturn0), and accelerate. |
| `skate_avoid` | (a helper) | (skateto's avoidance routine, a0: run through the RAM trampoline jmlvector, which is set) |
| `skateto` | `$9EA5A4` | Player o skates toward d0 / d1 (around the goals, led by his velocity), every $0C frames aiming again; slowing down to a stop (direction 9) near it (within 5, or 12 when fast; goalies only with goalieflags bit 1; the distance measured from the target plus the velocity's sides as ApproxLength left them, not the lead); the avoidance routine a0 may change the direction; standing with no direction he looks at the puck (that direction is not used). |
| `skatetopuck` | `$9EAFB5` | Player o skates to where the puck will be (around the goals, led by his hot spot and velocity), aiming again every 5 frames; a skater facing the puck (head on to its travel) at the right range from its carrier sweep checks instead (as player a3, who then also skates). |
| `off_spot` | (a helper) | (asswingo / asscentero: a new spot from the table row d2 (x, x spread, y, y spread) in temp3 temp4, unless the row is the same as last time and the random word is not small) |
| `off_zone` | (a helper) | (asswingo / asscentero: d2 = the zone of puck y d0 (lead d3) seen from the player's goal: 0 back, 8 mid, 16 past the blue line (unless offside), 24 past the goal line) |
| `asswingo_start` | (a helper) | (asswingo / asscentero / asscenterd's start: on offense the carrier gets asspuckc ($11) on top and the other team's puck makes it assignment def, on defense his own team's does; the clock stopped: donothing; then check4bench; a human is left alone; the first time, the timers (and with spots the last spot row) reset. |
| `asswingo` | `$9E8A6A` | Assignment 4: a wing on offense skates (avoiding the carrier) to a spot of the puck's zone (puck y led by its velocity), picked again as the zone changes or at random when his timer runs out. |
| `asscentero` | `$9E843C` | Assignment 6: the center on offense, the same with his own spots (CenterOSpots) by the puck y. |
| `asscenterd` | `$9E8573` | Assignment 5: the center on defense, halfway between the puck and his goal. |
| `assgoalietopuck` | `$9ED179` | Assignment $12: the goalie skates out to a loose puck (goalieflags bits 1-2 by where he is), while his team is not near it and it does not come at his net fast; the leg strength doubled meanwhile. |
| `ChkOffsides` | `$9EF78A` | Offsides, both teams: each team's flag cleared once it is back onside (d0 as given for the first's empty spots); when the puck crosses a blue line ($54 from centre) StartBreakaway, and with offsides on the team attacking that end (by the carrier, or the last player to touch it) is flagged when one of its skaters is past the line ($5E). |
| `ChkOffsides_other` | `$9EF833` | ChkOffsides with the puck in the bottom half: the line d0 negated (the skaters' line $0A below d0 as CheckBreakaway leaves it), the teams a1 / a2 swapped the other way. |
| `checkob` | `$9EC417` | The little offsides referee: with offsides on, while a teammate of player o is past the blue line ($5C) with the puck outside it, sflags bit 15 is set and the ref shown (PushRef 6, when no ref is up); once none is, the flag cleared and the ref erased (unless a ref window is up). |
| `chk4pass` | `$9EE89F` | A computer player o with the puck looks for a pass (1 in 2 or so without a threat): a random teammate, available, ahead of him or not too far back (and on his side of the blue line with offsides on), with no opponent closer to the puck in that direction. |
| `assshoot` | `$9EE146` | Assignment $13: a computer shot: the first time it starts (SetShotMode); with shot mode over, the assignment is over; meanwhile the swing timer temp2b counts down, releasing the shot button (d2 cbut) when it runs out (ShotMode, the pad direction from temp1). |
| `a2offsides` | `$9EC5F0` | Offsides on player a2 (a3 his partner in a2 / a3, swapped around the call as the original does): with offsides on, the puck past the blue line in his attacking zone and his team flagged. |
| `a2touchpuck` | `$9EC4F3` | Player a2 touches the puck: the last toucher (ltx / lty / ltplayer) and his team's last three touches (a new touch after the team's flag 4 raises the home crowd), then offsides; a called icing (by the player iflags names, X = x around it) unless the touching team may touch it; otherwise icing re-armed for him when the puck is in his half and his team is not short-handed. |
| `asspassrec` | `$9EE706` | Assignment $15: waiting for a pass. |
| `sopath_next` | (a helper) | (assbreakaway's .9: the next point of the path (x signed by its side), the distance for the one after) |
| `sopath_start` | (a helper) | (assbreakaway's .11 / .4x: the side to go in on, at random or by his stick hand, signed by his goal; close in (d0, his distance from his own end, past $2C) the path starts at its third point, straight in when he is near the middle. |
| `assbreakaway` | `$9EE463` | Assignment $10: a computer puck carrier on a breakaway skates the path to the net (no avoidance) and, once his facing turns from the path's (at random), shoots a one-timer at the goal's side; with the clock stopped he glides back. |
| `fo_find` | (a helper) | (Endfaceoff's .13: the player of team y0's six in position p, -1 for none) |
| `Endfaceoff` | `$9FB092` | Drop the puck (a3, the puck object): the face off sound, the face off over and the clock going; the winner picked at random by the two centres' pulls (fofdata2), the puck sent in his pad direction (fodir1 / fodir2) or at random toward his side (and then 1 in 4 at random anyway); the centre of the team it goes to touches it (a2touchpuck); the puck takes assignment $18. |
| `assnearest` | `$9E800D` | Assignment 1: the player nearest the puck who does not have it (a goalie: assgoalie; the computer carrier: asspuckc on top). |
| `CompLine` | `$9FDA53` | The computer picks team a2's line (a1 the other team): with a man more or less, of lines 3 / 4 (short) or 5 / 6 (the power play), the one with more energy; at even strength (the home team with a human on pad 1 and automatic changes, or the visitors: the second table) the first line of a list (by the score in the third period) with energy left, the home team's matching the other team's line when it has enough. |
| `CPgoalie` | `$9ED130` | The computer pulls team a2's goalie (a1 the other team) in the third period, behind by one or two with under a minute ($3D) left and the puck (y d1) in its attacking half: tmgoalie's high byte $FF, then setpersonel. |
| `return_goalie` | (a helper) | (ReturnGoalies' .1: team a2's pulled goalie back, unless a pad is on the team: then CPgoalie with the faceoff spot's y) |
| `ReturnGoalies` | `$9ED0FD` | At a faceoff each team's pulled goalie comes back (for a computer team, unless CPgoalie pulls him again). |
| `chk_goalie` | (a helper) | (ChkGoalies' .1: team a2 with the puck pulls its goalie on a delayed penalty, or a computer team by CPgoalie) |
| `ChkGoalies` | `$9ED0A8` | With the clock running, each team may pull its goalie (chk_goalie). |
| `puck_cross` | (a helper) | (pucknorm_move's .16: where and in how many frames the puck crosses the goal line d4 (the entry at y of puckcross: x clamped to the sidelines, frames; $FFFF frames when it moves away or not at all); returns d0 as left) |
| `pucknorm_move` | `$9EDCC3` | The puck (x) moves: the goal-line crossing times run down, and every 5 frames they are worked out again (puck_cross); a carried puck is touched and follows his stick (a quarter of the way to his hot spot, his velocity); icing and offsides; with the clock running, during a penalty shot a puck going back toward the shooter's end for $30 frames ends it, and a puck that has not moved for $78 frames is out of play (PenOOP); a puck at rest on the ice stops spinning; then its collisions (checkpuckcoll). |
| `pucknorm` | `$9EDCA8` | Assignment $18 (the puck most of the time): the first time, its timers reset (still puck $78, the penalty shot's count back); then pucknorm_move. |
| `chk4lc` | `$9FDBD0` | A computer carrier o (a3 after setpersonel) in his own end ($58 from his goal) changes lines on 1 in 4 (line changes on, not automatic; no change under way, his line tired: under $C01, no penalty shot): CompLine, setpersonel, the scoreboard, then he shoots the puck in (CompShoot: returns nonzero, his caller is ended too). |
| `asspuckc_chkdir` | (a helper) | (asspuckc's .chkdir, a skateto callback: when an opponent (led by his velocity) will be within $15 of where the puck goes (threat counted), the direction away from him, turned a step at random when that is right behind player o; otherwise d0's low byte as it was) |
| `asspuckc` | `$9E8292` | Assignment $11: the puck carrier (anyone else, or a human, leaves it). |
| `updatefaceoff` | `$9FAF1E` | The puck drop of player o's faceoff (temp1 the timer): while it runs, the drop frame 10 - (timer + 6) / 8 into fofdata in its last steps; when it is over the faceoff box is erased (its print position by fodropx / fodropy; the penalty shot's own box) and the frame counts restart. |
| `fo_spot` | (a helper) | (puckfaceoff2: a player's faceoff spot, d1 his position: from the table .19 (by how many his team has on the ice; the penalty shot's .20) into the offsets .21, his end's way round; the skaters' near the faceoff spot pulled toward the middle) |
| `pads_off` | (a helper) | (puckfaceoff2: every pad of team t but the one kept (keep) moved off it) |
| `puckfaceoff2` | `$9FA835` | Assignment $17 (the puck at a faceoff), object o, a3 the puck object. |
| `fo_compline` | (a helper) | (puckfaceoff's .4: the computer picks team a2's line (a1 the other), puts it on, the scoreboard) |
| `fo_line` | (a helper) | (puckfaceoff's .1: with line changes on and no pad on team d0 (1 home, 2 visitors), fo_compline) |
| `fo_pad_line` | (a helper) | (puckfaceoff's .5 / .6: automatic changes: the computer's line for team a2; else player p (an object * 2) opens his team's box, and when it is open its timer (home temp1 / temp3 of the puck a3 $258, visitors temp2 / temp4 $168) starts with him as its owner) |
| `fo_pad_player` | (a helper) | (puckfaceoff's .10-.12: the player of the first pad of team a2 (c1playernum by padside)) |
| `fo_box_timer` | (a helper) | (puckfaceoff's .t1xxxxxxx: the line change box timer of the puck a3 at d0 (0: temp1 / temp3 for the home team, $20: temp2 / temp4 for the visitors): its owner's box closed meanwhile: the timer stopped and the box drawn again; run out: the box still open makes his choice row 0 (lcfound) and draws it again) |
| `puckfaceoff` | `$9FB355` | Assignment $1C (the puck before a faceoff), object o, a3 the puck object (d0 and a0 as the caller left them, for pucknorm_move). |
| `setpads` | `$9BC174` | Pad marker slot d4 follows player o (its number, stored as a word). |
| `Acheck` | `$9EA2CB` | Hook or hold: player o is locked in the hook animation, or the hold when the best player a0 (if any) faces within the right range of him. |
| `setpassmode` | `$9EE6D2` | passdir = player o's facing (and sfspdir), unless he is the goalie with the puck just behind the net (x -16..15, y $108-$10F). |
| `Findhittype` | `$9EE698` | Player o's swing for direction *d0 (made relative to his facing): 0 forehand, 1 backhand. |
| `CalcAngleOffset` | `$9EE0CD` | The cross product of the goalie vector d0 / d1 and (d3 / d4 - the puck), divided by d2 (*d3 is left the point's x from the puck). |
| `getlchoice` | `$9FD7B2` | The line for choice d0 (0-2) of team a2 by the man advantage, from the table: choice + 3 * the current line, 21 on when the counts on the ice differ and 21 more when the other team has more (or as many); -1 for none. |
| `holdplayer` | `$9EA24C` | Hold button: unless player o is the goalie or locked in his burst, the opponent with the lowest distance + 16 * facing difference (folded to 0-4) is a0 (none: -1), and Acheck hooks or holds. |
| `SetShotMode` | `$9EE17A` | Start a shot by player o: no pass direction, shot mode (sflags 8), aimed at the goal, speed 15, the forehand or backhand shot. |
| `Sweepcheck` | `$9EA242` | Start a sweep check by player o (held in pf2aip). |
| `burst` | `$9E81F9` | Check button: a speed burst along the facing of player a3 (o, the same player, for the lock and the energy): energy * $233 >> 16, after paying $CC of it with line changes on (automatic ones: from a fixed $1000). |
| `faceoffinput` | `$9FB30F` | Pad input for player o taking a face off (assignment $17): the pad direction d0 goes in fodir1 (fodir2 shooting at the top goal); unless he is swinging already, B (d1 $8000) starts the swing, otherwise the ready pose. |
| `shotdiradj` | `$9EDFF7` | A computer shot by player o (a3 after the aim) goes to the open side when the goalie, where he will be, covers the middle: passdir 1-4 or 5-8 at random, 0 when he does not, 8 with no goalie. |
| `dopass_swing` | `$9EEACC` | The pass once the puck's velocity is set (dopass, assdumpin): a goalie never passes back toward his net; the pass swing (SPAgswing, or forehand / backhand), locked; a human goalie stops and his pad picks its player again (findpc, a3 as given). |
| `pass_launch` | (a helper) | (dopass_count / its tail: the pass's speed d5 (spread by the passer's accuracy) along d2 / d3 over the distance d4, the target, the waits, the pad) |
| `dopass_count` | `$9EEB2E` | The pass to receiver a0 (player o's team, a3 the passer): counted (clock running), the other goalie's timer raised, the receiver gets asspassrec ($15) and is aimed at where his stick will be (his one-timer frame's hot spot) after the puck's travel, at a speed by the distance. |
| `dopass` | `$9EE9BF` | Player o passes in passdir: the puck goes loose, the pass speed by his accuracy (a goalie 7); the teammate (by a3's team... |
| `passmode` | `$9EE9A3` | Pass mode input for player o: the pass button changing (d2 $8000) passes; a pad direction (d0, bit 3 clear) becomes passdir and passes. |
| `doshot` | `$9EE220` | The stick is at the puck: player o shoots toward the goal (shotdiradj may aim a computer shot at the open side; a3 after). |
| `ShotMode` | `$9EE1C3` | Shot input for player o (d0 the pad direction, d2 the changed buttons): only the carrier in a shot animation; at its last frame ($18) he shoots; the pad aims, the windup builds the speed by the frames elapsed, and letting go of the button jumps the animation on ($1C - the frame, shooting at $18 or past). |
| `setlccords` | `$9FE089` | The print position (printx / printy) and size of team a2's line change box: its side's spot (setlccordsPos, the other side with the teams switched, 21 rows apart; the print's carry goes into the add), 12 wide (returned) and 6 high (*d1; 5, a row lower, when choice 2 has no line). |
| `SetLCmode2` | `$9FD325` | Draw team a2's line change box: its area erased (but over the faceoff box) and framed (in the upper half a plain box first; in the lower half sf3llcs set), then a row per line choice (all of them while it is open, else the chosen one with the current line): the button letter, the line's name and its energy bar; then the team's name. |
| `SetLCmode` | `$9FD2FE` | Open the line change box of player o's team (line changes on, not automatic; not open yet): tmflags bit 3, the pass and shot directions off, SetLCmode2. |
| `lcfound_close` | `$9FDE13` | Close team a2's line change box: its area erased (but over the faceoff box; sf3llcs cleared in the lower half), then the scoreboard. |
| `lcfound` | `$9FE001` | Team a2 changes to the line of row d2 (remembered; none there: nothing): player a3 is done with the box (pflags bit 3), his team's box closes (tmflags bit 3, no pad), its line set and put on the ice (setpersonel), lcfound_close. |
| `lineinput` | `$9FD733` | Pad input of player o while his team's line change box is open (d1 the new presses, d0 the direction): a redraw request (tmflags bit 0) closes and reopens it; Y, B, A pick row 0, 1, 2 (lcfound, by a3); Select closes it keeping the line; otherwise he skates when done with the box and free (with the d0 a redraw left). |
| `goalie_input` | (a helper) | (doinput's .13: a goalie skates unless his flags say he is out of his crease, not covering) |
| `take_goalie` | (a helper) | (doinput's .17: the pad d4 takes its team's goalie, with manual goalies) |
| `locked_input` | (a helper) | (doinput's .22: a locked player (or the pass receiver) can only change player, take the goalie, or with A give the pad to a free computer teammate the pass goes to, asking for a one-timer) |
| `doinput` | `$9FC1B9` | Pad d4's input for player o (d0 the direction, bit 3 none, turned on the horizontal screen; d1 the new presses, d2 for the pass and shot modes, d3 held): Start pauses; the faceoff; the line change box (when open and his pad's or nobody's); Select opens it; the carrier passes, shoots, dumps in (Y) or skates; others hold (Y), take the goalie (X), change player (B), take a defenseman (L / R), burst (A) or skate. |
| `view_step` | (a helper) | arithmetic shift right by 4, at least 1 away from 0 (checkwindow's step toward the target) |
| `count_down` | (a helper) | a - step, at least 0 (the count downs by timestep and impactstep) |
| `run_assignment` | (a helper) | (the jsl jmlvector of updateplayers: assignment addr, an asstab entry, for object o of team a2; a3 = o) |
| `freezewindow` | `$9FBA97` | Lock the view at its current position |
| `checkwindow` | `$9FC090` | Move the view so the screen follows the puck (or the lock position yc1 / xc1): the puck's y plus its lead (yleader, toward the goal it is heading for), a sixteenth of the way a frame once past a dead zone, within the rink's limits |
| `updateanim` | `$9EA419` | Step object o's sprite animation, looping or ending it: the frame list for its facing direction (mirrored for flipped players, turned for the horizontal rink), the next frame when its count runs out; at the end the animation is cleared (unless it loops, word $10) and the bench animations take the player to the door |
| `updateplayers` | `$9F8686` | Every object's frame: first (from the last object) the animation, the counts, the velocity decay and the move by it (the puck's height with gravity and its bounce sound), the distance and direction to the puck, the nearest player of each team, the icing waved off; the nets kept in their ends; then (from the first) each player's pad input and assignment, goalies kept out of the goal posts, collisions (players crossing the goal line beside the net put back) and the impact decay (an absent object's impact = its position); the puck kept within the boards |
| `SetSPAUpdate` | `$9EA403` | Start sprite animation anim on object o (as SetSPAFromA) and update it at once |

### `game/play/penalty.c` (69)

| Function | Original | What it does |
|---|---|---|
| `AdvanceStringPtr` | `$9ED323` | a1 = string n of the string list a1 (each string starts with its length word) |
| `ChkShotStat_add` | `$9FC975` | Add amount to the team stat whose home word is at stat (the visitors' is the next word), for player o's team |
| `AlignBigNumber` | `$9F9DFB` | Move printx left by the width of string a1 in the big font after its first character (one column for a 1, else two) |
| `ClearPenaltyBuffer` | `$9EC107` | Clear the list of called penalties |
| `RemovePlayerFromList` | `$9EA6FA` | Remove the box list entry before index a0 by moving the rest down to the end marker (bit 7); returns a0 stepped back |
| `reenergizeteam` | `$9FDD9D` | Give every player of team a2 full energy (the out-of-the-game check that follows can never pass) |
| `chkatop` | `$9EB198` | Add to the attack zone time of the team with the puck past a blue line |
| `InProgress_ap` | `$9EB6F7` | Count team a2's players kept off the ice by penalties (the penalty flag $2000 cleared on the way) to set how many it has on the ice (at least 4) |
| `PenBoxSpot` | `$9EBEA1` | The penalty box y spot of player o (r2): +-11 by team times (his team's players in the box, at most 2, plus 3) |
| `GetLowestPen` | `$9EBB92` | The power play time left (d0), from the box times: team a2 on the power play, a3 the shorthanded team |
| `AvgCline` | `$9FDD19` | The average energy of team a2's players on the ice (d0; 0 when none) |
| `limitfo_t2` | (a helper) | Keep the faceoff spot away from the blue line for every called penalty (from PenBuf's first word, zero or not, to the next zero word) |
| `limitfo` | `$9FB601` | The faceoff spot (fox, foy) after a penalty call, checked against each penalty in PenBuf (limitfo_t2): one may pull it out of an end zone to the neutral-zone dot on that side |
| `getlinee` | `$9FDF0C` | The average energy of line d0 of team a2 (d0): the players of the line in priolist's order, as many as the team has on the ice |
| `AddPenalty2` | `$9EB88A` | Call penalty d0 on player o without the checks: the crowd reacts (home penalties of PenGoal and up), and the first free PenBuf word (low byte 0; $34 entries are passed over) gets (o, d0), taken back if the penalty has minutes and he already has one (pflags $4000) |
| `ChkShotStat` | `$9EF213` | If a shot was taken (sflags $1000, with the clock running), count it: team shots (by the shooter's team), power play / shorthanded shots and shots by period (by his side: objects 6 and up are the visitors), the shooter's shots (but a goalie number) and the other goalie's shots against; then excite the crowd |
| `AddPenalty` | `$9EB85E` | Call penalty d0 on player o, unless the clock is stopped, a penalty shot is on, or penalties are off (icing and the penalty shot always; offsides only with offsides on) |
| `InProgress_coin` | `$9EB6CC` | The end of the referee's work: unless a penalty shot is on, count both teams' players on the ice |
| `db_w16` | (a helper) | (PenGoalStuff's store through the data bank: only the bytes in its WRAM, below $2000, are written) |
| `PenGoalStuff` | `$9EB94C` | After a goal by team a2 on team a1: PenBuf's entries without minutes or not for the scoring team become $34 (period 3 clears it), then, when the scoring team had more players on the ice, the first scored-on player in the box whose penalty is not coincidental comes out. |
| `InProgress_exit` | `$9EB6B8` | The penalties are over: end the penalty mode, the puck to its faceoff assignment, then InProgress_coin |
| `Fprint` | `$9ECFC6` | Print string d0 of the string list a1 (each string starts with its length word); returns a1 past it |
| `printscores1_per` | `$9F9D80` | Print the label of the current period (overtime's one column to the left) |
| `printscores1_ps_name` | `$9FBD66` | Print team d3's name (0 home), then move down two rows |
| `ClearPenaltyMessage` | `$9EC913` | Clear the penalty message area on the horizontal rink (a 24 x 3 frame) |
| `linebar` | `$9FDFA6` | Draw the energy bar of team a2's line d0: five bar characters by its energy (at most $1000); returns that energy (left in d0) |
| `pplpen` | `$9FA21E` | One penalty box line (above row $0B): player d0 / 2 of team a2's number and his time left |
| `printscores1_ps` | `$9F9DB0` | Print team a2's (d3's for the name) name, then its score mod 100 as two characters |
| `DispPen` | `$9FA1B8` | Team a2's penalty box lines: the players with a running penalty clock first, then the others |
| `setscore_goals` | (a helper) | SetScore's goals for team d0 against team d1: the weights of d0's row (bits 4-6 of its team record's +8 word) plus those of d1's row (bits 0-2) of the SetScore_5 table; one of the 4 picked |
| `SetScore` | `$9F9EB7` | Play one period of playoff game a0 / 2 with random goals; after period 3 a game within a goal goes to overtime (period 3 again, flag 2), or in period 4 a tie goes to team 1 |
| `prefmes` | `$9EC160` | Print the message for penalty d0 (negative: only clear it): on the vertical rink in a frame sized to it, on the horizontal one in the message area. |
| `UpdateScores` | `$9F9712` | Update the other playoff games of the round: the ticker starts at the last; every game but the user's loses its highlight flag (2) and, unless over (4), plays a period (SetScore) |
| `InitScores` | `$9FC833` | Playoff rounds 0 and 1: the other games of the round not over get period 0 and 1 to 3 SetScore periods |
| `PushRef` | `$9EFA36` | Draw referee frame d0 (8 rows of 7 tiles from RefsMap, RefsMapHor on the horizontal rink) in the referee window; frame $40 erases the window and clears the message |
| `SetPA2` | `$9EF991` | Step the referee animation of penalty RefPen: its next (delay, frame) pair from the animation the penalty's record points to (a negative pair is the last: RefStep -1); RefCnt = delay * 8, the frame drawn. |
| `PenShotChk` | `$9EC042` | A = -1 when the player hit (a2) is the puck carrier on a breakaway (outside highlight mode; the breakaway flag checked again): a penalty shot instead of the penalty; else 0 |
| `SeatPenaltyBox` | `$9EBF11` | Put every player in the penalty box (negative position) at his box spot on the sideline, with his box assignment (not in a penalty shot, its set-up or a shootout) |
| `setup_team` | (a helper) | Team a2 (a3 the other) for a period: full energy, line 0; with line changes on, a3 on line 3 when it has more players on the ice than a2, line 5 when fewer |
| `SetupTeamForIntermission` | `$9FE03F` | Before a period: reset the bench, newperiod, refill both teams' energy and choose their lines |
| `EASNLogo` | `$9FBEFD` | Paste the EASN logo into the rink's text map and widen the dirty range, unless a power play is on |
| `releasepl` | `$9EC071` | Player d0 / 2 of team a2 is out of the box: the team's first object with no position takes his place on the ice (the next position of priolist, the goalie-pulled list when the goalie is out); returns him |
| `updatepwrplay_clr` | `$9EB851` | Power play over: the EASN logo back |
| `updatepwrplay` | `$9EB735` | The power play box: the time left (the warning sound in its last seconds) and the team on the power play (not in a shootout); the logo back when the teams are even |
| `pentime_release` | (a helper) | Release player d0 of team a2 when his box time has run out (below 6 and zero) |
| `pentime_team` | (a helper) | One second of team a2's box list: the first two players with a running clock count down (the others wait), a clock at 0 leaves the list; an untimed one (bit $4000) counts down too and, with bit $0800, ends the team's pass ($1000 left when it had no bit $1000, else 0). |
| `updatepentime` | `$9EBF44` | Once a second of running clock (not in a penalty shot): the attack zone and power play times, then both teams' penalty box clocks; sflags3 bit $20 marks the second |
| `printscores1` | `$9F9B36` | The scoreboard: on the vertical rink the period box (the shootout round in a shootout; else the period, the logo and the power play box) and, without the lower line change box, the score boxes; on the horizontal rink the period and both scores in the big font |
| `ClrHor` | `$9FB9A4` | Back from the horizontal rink to the vertical one: a fade when the screen is lit, the sprites sorted, the text map cleared and the scoreboard redrawn (not in a pause), the palette sent |
| `USBoard` | `$9FA130` | The scoreboard with both teams' penalty boxes |
| `SetHor` | `$9FBA17` | The horizontal rink (the scoreboard screen): the puck at half speed, the screen blanked, the scroll and rows reset, the sprites sorted, the stats report drawn |
| `SetPA` | `$9EF969` | Start the referee animation of penalty d0 and its message (the goal box for a goal), the message time long ($3C frames on the horizontal rink); its first step |
| `Stop4Pen` | `$9EB2C9` | Stop play for the penalty before a0 in PenBuf (unless the clock was already stopped): the faceoff spot (none for a goal; the last touch for offside passes; else the puck, at the end of the icing team's goal for icing) kept inside the faceoff circles; then the replay, the whistle and the referee |
| `checkfornewpen` | `$9EB1E5` | Go through PenBuf's new calls (not on the horizontal rink): a penalty that stops play does so (Stop4Pen) unless its team's opponent has the puck (a delayed call: the referee's arm, once); once seen (bit 7 of its player byte) a call sets the fight count down from its record |
| `UpdatePA` | `$9EF913` | The referee animation's next step when its count runs out; when the message time runs out, the message line cleared and the penalized player's name printed there, centred |
| `penalty_to_box` | (a helper) | Send the player of the new penalty entry before a0 to the box: the team's penalties and minutes, the player's minutes, the penalty log, his box time ($2000 \| minutes * 60, keeping bit $1000), his box list slot and the box assignment; then the referee and the scoreboard |
| `InProgress` | (a helper) | The referee's work on the last PenBuf entry (a0 just past the first one): a call already seen stays unless its player has left the ice; a new one (bit 15) sends its player to the box, starts a penalty shot or only shows its message (the stars first for the end of the game). |
| `chkprogress_find` | (a helper) | Any penalty left in PenBuf: the referee's work on it (InProgress), else the end of the penalty mode |
| `chkprogress` | `$9EB3BC` | During a penalty: the fight count down; when it runs out a call with a message (minutes, not 1) turns to the horizontal rink with the referee; else, the players in the box seated, once the referee is done the next penalty |
| `PenaltyManager` | `$9EB188` | Every game loop: the penalty box times, new calls, the referee |
| `ticker_box` | (a helper) | The ticker box for playoff game d3 (not one over or with a highlight pending: 0): a frame with the round, the period and both teams with their scores |
| `NewTicker2` | `$9FA461` | The next ticker game (TickerNum, counting down; the user's game skipped) shown, while there is one |
| `NewTicker3` | `$9FA476` | The ticker box for game d3, or the next game when it is over or has a highlight pending |
| `NewTicker` | `$9FA458` | The next ticker score once the clock is at 8:00 or less |
| `ticker_start` | (a helper) | The score ticker from the last game of the round when it has none (or it is at game 0 and so is the user's game); then the next game |
| `DoHiLights` | `$9FA636` | Playoffs: the highlight of each game of the round still pending (and not played out) |
| `Intermission_tick` | `$9FA393` | In the intermission (demoflag): the next ticker score every $150 frames, until the ticker is back at its first game: then the highlights. |
| `Intermission` | `$9FA2A6` | Between periods: both teams set up, the ticker started, the pause controller the first pad with a team (none: 0), the scoreboard and the pause menu until it is left |
| `SetHor_rink` | `$9FC896` | Clear the text screen and draw the horizontal rink's scoreboard background: the screen erased between its two print positions, the two halves of the rink map (rinkmapbuf, columns 0 and $10 of row $55, 32 by 28) to the screen maps at $0400 and $0800, then USBoard. |

### `game/play/period.c` (12)

| Function | Original | What it does |
|---|---|---|
| `GetTeamRating` | `$9D87B5` | The team rating of team a2: byte 4 of its scouting report. |
| `ClearGameStatBraces` | `$9C8F36` | Erase the braces around the visible game-stat rows / player-stat rows / other-scores rows |
| `SendGameStatsPage` | `$9C8F7D` | Send the stats page rows shown (from listtop, listrows + 1 of them) to VRAM $0180 |
| `teamstat_value` | (a helper) | Game stats: one team's (a2) number of stat row curitem at printx (centred): a count, a ratio as a percentage, a pair "a/b", or a time, from the TeamStatSources entry (penalty rows only with penalties) |
| `DisplayStats` | `$9C8F9B` | The game stats page: each team stat's name (the list without penalties when they are off) and both teams' numbers |
| `ManualGoalieMenu` | `$9FE3FF` | Goalie control (manual or automatic): A / B / left / right switch it for the pause controller's team |
| `gamestats_loop` | (a helper) | The game stats screen's braces round the rows shown (when rows are hidden above / below), the page sent; then up / down scroll the rows (a row at a time, the IRQ split moving), Start leaves |
| `StatsReport` | `$9C8D2E` | The game stats screen: both teams' blocks and the team stats (6 rows of 16, 11 without penalties, at a time) |
| `period_column` | (a helper) | One column of the period stats at printx: the period's label (none past lastperiod), the total's, then both teams' goals (curitem 0) or shots in that period, and the title of what is shown |
| `DrawPeriodStats` | `$9F8064` | The period stats page: the pictures, the title, both teams' blocks and a column per period and the total |
| `PeriodStatsScreen` | `$9F8000` | The period stats screen: Y switches goals / shots, Start leaves |
| `updatePPTeamTime` | `$9EB1C9` | The team with more players on the ice gets one more tick of power play time |

### `game/play/shootout.c` (14)

| Function | Original | What it does |
|---|---|---|
| `InitShooters` | `$80CDC8` | Shootout: the 8 bytes of team a2's starters list (team data + its word at +$0C) as its shooter list |
| `SetShooter` | `$80D2BD` | Put the player at the cursor in shooter row curitem of team a2's list; if he is already in rows 1-6, the player the row had takes his place |
| `ShootoutWonBy` | `$80C943` | "<team> Wins!" once the shootout is decided (a 6-row frame centred on the name); then the whistle (PenNoText; $0A while undecided, outside a shootout the caller's d0) on player o |
| `DrawShootersBack` | `$80CB5C` | The shooters screen: the top cleared, the frame outline with "Lineup", the background map into text map 2 |
| `ShootersBackground` | `$80CC1B` | The shooters screen's title: the background redrawn (drawlinesback, then the band IRQ line back), the label, team a2's block and the pictures |
| `ShootersSetup` | `$80CAE1` | The shooters screen's top: cleared, the frame outline with "Lineup", the title |
| `PrintShooterNames` | `$80CC89` | Team a2's shootout list: the goalie then the five shooters, a row each with its mark, the cursor row (curitem) highlighted and its player the home matchup picture (for teams from $18 the frame of the direct page's p1 freed); then the title |
| `PrintShooterRows` | `$80D21C` | The pick list's rows: nine players from picktop + 1 (eight from line 6), the cursor's highlighted, each with attribute d4 of mask (goalies' from the goalie list) |
| `PrintShooterList` | `$80D004` | The shooter pick list: the attribute column's title (DispAttribCtr, kept inside the columns), the scroll arrows, the rows |
| `ShooterSelectList` | `$80CE41` | Pick a player for shooter row curitem of team a2 from the skaters, nine at a time (up / down move the cursor and scroll, left / right the attribute shown); A / B take him (SetShooter), Start cancels |
| `PickShooterRow` | `$80CE0A` | Shooter row curitem of team a2 picked: the goalie row steps to the next goalie (after the last the first), a shooter row opens the pick list |
| `ShootoutShooters` | `$80CA0B` | The shootout lineup of the pausing team (when a pad plays it): up / down the row, A / B pick for it, Start leaves (the goalie set from the list) |
| `PlayShootout_t1` | `$80C898` | A shootout turn of team a2: a period started for it, the pad that plays its shooter, the penalty shot with this round's shooter from its list (rounds past five from the top again), then the game loop until the turn is over |
| `PlayShootout` | `$80C81C` | The shootout (line changes off): both shooter lists, then a turn for each team (visitors first) every round, five rounds and then on until the score differs |

## Screen: the picture (printing, the rink, sprites, the camera, replays, the crowd, cards, the scouting report)

### `game/screen/cards.c` (22)

| Function | Original | What it does |
|---|---|---|
| `picboxmap_add` | (a helper) | Add base to every word of the map unpacked at picboxmap (its width and height first, the words from +6) |
| `card_starters` | (a helper) | The six starters' roster bytes of team a2 (its team data + the word at +$C) |
| `card_starter` | (a helper) | (PlayerCards: whether roster player p is one of the six starters at a3, whose cards come first) |
| `CalcAttrib` | `$9F992E` | The rating (0-100) of player d0 of team a2 (0 home, 2 visitors) from the attributes of mask d4+2 (bit 15: the weighted overall rating, r2 = 0 skater / 16 goalie weights; otherwise the one attribute), with the hot / cold table when usehotcold is set. |
| `PrintCentered` | `$9FF345` | Print string a1 centred on column 16; returns a1 past it |
| `pic_not_starter` | (a helper) | (.9: 0 when curplayer is one of the six players of team a2's first line (team data +12), else 1) |
| `pic_mirrored` | (a helper) | (.6: whether the picture is mirrored: a first line player never; else bit 0 of his overall rating (column 4 for a goalie, 5 for a skater; inverted for a goalie)) |
| `paste_mirrored` | (a helper) | (.5: paste map [p0] into the map at [p1] (r7 entries a row) at column x, row y, mirrored (each row from the right, the flip bit set), adding a to each entry) |
| `pic_map_and_colours` | (a helper) | (DrawPlayerPicture: the photo map in the text map at x, y (tiles from picchars, the visitors' $30 on), then the team's photo palette (colours $50 home, $10 visitors)) |
| `DrawPlayerPicture` | `$9DC9A8` | Draw the picture of player d0 of team a2 at column x, row y: his photo (PlayerPhotoTable, 26 a team) unpacked and made planar in the team's picture buffer ($7F6F00 home, $7F6900 visitors), mirrored for some players; with matchupphotos the matchup photos (LoadMatchupPhotos) instead |
| `card_print` | (a helper) | (print string a1 centred on column 16 at row y; a1 past it) |
| `card_strengths` | (a helper) | (DrawPlayerCard: the strengths label, then the names (from list a1) of up to three strengths, the second and third only when rated 80 or more) |
| `DrawPlayerCard` | `$9DBE79` | Draw the current card of team a2: curitem 0 the team card (the team name, the six line slots and the team rating), 1-6 a line slot's player, 7 and up roster player curitem - 7 (the number and name, the position group, the strengths and the overall rating); then the picture and the card colours |
| `SetCardTeamColours` | `$9DC2DE` | Copy the card team's two colours (a2 nonzero: colours $67-$68, else $77-$78) to colours $4E-$4F |
| `DrawLogoSprite` | `$9DDFBF` | Draw the logo of team a as sprites at x, y with its LogoPaletteTable palette |
| `PrintSmallLetterSprites` | `$9C9590` | Draw string [a1] with the small letter sprites from x, y: letters, digits and '.' (2 wide), a space 3 wide, other characters skipped |
| `PrintLineSlotName` | `$9DC2B8` | The short name of team a2's player in line slot y (a3) in small letter sprites centred on x, at printy |
| `LoadPlayerCardScreen` | `$9DC697` | The player card screen's graphics: the team logo sprites, the screen registers, the fonts (remapped), the team block, card back and frame tiles to VRAM, the card picture map, the card back map into text map 2, the palettes, the "switch cards" text, the sprites and the scroll |
| `PlayerCards` | `$9DBD4F` | The player cards of the pausing team: right / left the next / previous card (the team card, the six line slots, then the roster without the starters), Y the other team, Start leaves |
| `AddStrength` | `$9DC3DB` | Insert rating column d1 of player curplayer of team a2 (GetPlayerRating) into the best-three list strengths (column, rating pairs) |
| `FindSkaterStrengths` | `$9DC2FB` | The player card strengths of skater curplayer of team a2: the best three of eleven rating columns |
| `FindGoalieStrengths` | `$9DC374` | The player card strengths of goalie curplayer of team a2: the best three goalie rating columns (columns 8 and 9, or 7 and 10 when column 4 is under 50) |

### `game/screen/crowd.c` (7)

| Function | Original | What it does |
|---|---|---|
| `Create_HotCold_Table` | `$9FE0E8` | Fill the hot / cold table (both teams, 624 bytes, from the end down) with random values -12 to 12: a random 0-13 limited to 12, negative when bit 4 of the seed is clear after it |
| `StartBenchCheer` | `$9C9FD5` | Start bench a cheering, unless one already is |
| `InitCrowdExcite` | `$9B9B6B` | At a period start (not overtime): crowd excitement $10 per playoff stage |
| `LowerHotColdTable` | `$9BA89F` | While HotColdRaised is not 0: the first four bytes of each of the 52 hot / cold entries go down by 2 |
| `UpdateBenchCheer` | `$9C9FED` | Each frame: step the cheering bench through its frames, and end it ($FFFF) after its rounds |
| `Crowd_Noise_meter` | `$9B9BA5` | The crowd meter (d0): sqrt(CwdExciteLvl * 4) + 65, at most 128; the peak and the running average |
| `Crowd_Noise` | `$9B9BF8` | The crowd meter into CurCrowdMeter: 110 dB or more sets CrowdLoud; under 105 dB a loud crowd's raised hot / cold values come down |

### `game/screen/replay.c` (10)

| Function | Original | What it does |
|---|---|---|
| `next_record` | (a helper) | The next replay frame record after a4 (the buffer wraps round to its start; suba4 has its own copy) |
| `updatereplay` | `$9BA8DB` | Save this frame into the replay buffer (through the work RAM port): each object's position, frame and flip bits, the roster numbers and positions, the puck's heights, the sounds, the pads' objects, the crowd, the view. |
| `RestoreReplayFrame` | `$9BB19D` | Put the replay frame record at a4 back into the objects, the puck, the sounds and the view (through the work RAM port; the view stays when sflags bit 5 is set) |
| `adda4` | `$9BB136` | Move the replay one frame forward from a4 (frames elapsed d7 = 0): not onto the record being written; returns the new a4 |
| `suba4` | `$9BB0C5` | Move the replay one frame back from a4: not past the buffer's start unless it has wrapped, nor onto the record being written; returns the new a4 |
| `ease` | (a helper) | (ReplayMode: the camera's step from cur toward target, at most step) |
| `clamp_h` | (a helper) | (ReplayMode: the view's limits, x -64 to 64 and y -199 to 256) |
| `clamp_v` | (a helper) | (ReplayMode: the camera's y, replayVpos, kept to -199 .. |
| `replay_sound` | (a helper) | (ReplayMode: replay recorded sound s (its low byte, $FF none): a streamed one from $16 on, else queued with its volume, and a voice it starts in ADSR gets the recorded pitch) |
| `ReplayMode` | `$9BAA60` | The instant replay: from the start of the recorded play (half way back to where the replay buffer's frames restart), frame by frame with the control panel. |

### `game/screen/scout.c` (23)

| Function | Original | What it does |
|---|---|---|
| `scout_team_strings` | (a helper) | (team data a1's strings: city, name, nickname, arena) |
| `matchup_rating` | (a helper) | (ShowMatchups: team a2's matchup player for the line slot curitem into curplayer, his rating printed; returns it) |
| `GetMatchupPlayer` | `$9DC973` | The roster slot (from 0) of team a2's player for matchup k (centre, wings, defence, goalie). |
| `matchup_logo` | (a helper) | (DrawMatchupPictures: the box at column x of the logo f (palette by LogoPaletteTable into colour y, the sprite drawn with attribute a at sx), its picture box cleared and the logo box map pasted into textmap2) |
| `matchup_player` | (a helper) | (DrawMatchupPictures: player d0's picture for team a2 at column x, its box map pasted) |
| `DrawMatchupPictures` | `$9DDDB3` | Draw the two 6 x 6 pictures, visitors at column 1 and home at $18: a team logo (setupvis setuphome) or, when that is negative, the matchup player's picture (matchupvis / matchuphome; the team a2 as the caller left it, or the other box's when both are pictures); unless the player stats are up, the pictures' tiles to VRAM (when one is a picture) and both text maps sent. |
| `scout_rank` | (a helper) | (ScoutingReport's .t1xx: player d3's hot value d0 into team y's three hottest and three coldest, the ones below moved down) |
| `scout_team` | (a helper) | (ScoutingReport's .11: team a2's players' hot values, rating minus base rating of the first rating column: their sum, and the hottest and coldest) |
| `scout_hotcold` | (a helper) | (ScoutingReport's .10: the hot and cold players of both teams; the hotter team (none when the sums are within 4) and whether by much (8)) |
| `scout_list` | (a helper) | (ScoutingReport's .12: the commentary's paragraph list (from InitScoutingDisplay's a0): $1D, $1C, the hot team's $1E, $1F, $20 when the teams differ, $21, the end; no delays, the screen's time) |
| `ScoutingReport` | `$9D8129` | The scouting report screen: both teams' hot and cold players, the matchups (paged by the pads' Y / B, A and on a timer) and the scrolling commentary (X starts it again; down held: no delays), until Start or, in the demo, its time runs out. |
| `ScoutNamePos` | `$9ECDE1` | Where a scouting report name is printed: the record's position, column $14 for the home side |
| `ClearPictureBox` | `$9DCCBA` | Blank the 6 x 6 picture area at column x, row y of the text map |
| `ScoutTypeWord` | `$9ECB2B` | Type the word at a1 (d1 characters) at the report's line column: on a new line when the column d0 is past 20, its leading spaces dropped at the start of a line |
| `ScoutPlainWord` | `$9ECAB7` | Copy the next plain word of the script a2 (with the spaces after it) into mesarea and type it at column d0; a "," or "." adds 40 frames to the report timer. |
| `GetTeamCity_home_3` | `$9ECD7C` | (ScoutTypeText's .3: type the string at a1 as one word, a word more counted in PlayerScrollCtr; returns the column d0 moved on by its length) |
| `ScoutTypeText` | `$9ECD36` | Type the string at a1 word by word from column d0, the words wrapping at 20 columns |
| `ScoutAppend` | `$9ECD19` | Type string a1 (copied to a fresh mesarea) from column d0 |
| `ScoutExtremely` | `$9ECC71` | Script code %e: "extremely " when the hot / cold totals differ by 8 or more (hotextreme), else the script's next word (a2 past the code) |
| `ScoutHotTeam` | `$9ECCA1` | Script code %t: the full name of the hotter team, then " has" (" have" for the two New York teams) |
| `ScoutPlayerName` | `$9ECBE8` | Type the name of player d0 of team a2 (0 home, 2 visitors) from column x |
| `ShowMatchups` | `$9D8489` | The matchups box of the scouting report: both team blocks; for the line slot curitem (0 the teams) the two players' pictures, ratings and names (or the teams' logos and overall ratings), then the slot's name and who has the advantage (arrows up / down for the visitors, or none) |
| `LoadScoutingScreen` | `$9D87D7` | Set up the scouting report screen: fade out (the team logo sprites unpacked meanwhile), the PPU's maps and sprites, the small font, framer, team block, outline, menu back and Ron Barr tiles, the menu back and Ron Barr maps in text map 2, the palettes, a clear text map, the sprites and the picture map |

### `game/screen/title.c` (31)

| Function | Original | What it does |
|---|---|---|
| `TitleDelay` | `$9DB884` | A busy loop of x passes, none once a pad press has ended the title (titleskip) |
| `StartCredits` | `$9DB9F7` | Reset the credits state |
| `TitlePathY` | `$9DB934` | The y scroll of title path step a: $1B0 - (a * $50 / 256) / 5 |
| `TitlePathScale` | `$9DB8A1` | The mode 7 scale of title path step a |
| `ShuffleTitleTeams` | `$9DBCF8` | Fill titleteams with the 26 team numbers in random order |
| `NextTitleTeam` | `$9DBD36` | The next team of the shuffled title list, shuffling again after the 26th |
| `TitleZoomIn` | `$9DB7A0` | Title: the mode 7 scale down to $100 a step at a time |
| `TitleWindowWipe` | `$9DBCB3` | Title: count BG1HScroll down, moving the window edges each step, then close them |
| `title_zoom_out` | (a helper) | Title: the mode 7 scale up to $200 while the BG1 scroll moves to ($1E8 / $120, $140) |
| `TitleWait` | `$9DBCEB` | Wait x frames, unless a pad press has ended the title |
| `DrawTitleSprite` | `$9DB754` | Draw sprite a of the title set at its position from the _1 / _2 tables |
| `TitlePathX` | `$9DB8E6` | The x offset of title path step a: sin(a) times a growing amplitude (a * 10 / $50 + 1), then a straight run |
| `PrintStringSprites` | `$9C9415` | Draw string a1 (from its first byte) as big letter sprites centred on x, at y |
| `titlelogo_map` | (a helper) | The title logo's map header at titlelogobuf from the map at p0 (width, height, $10, then each tile + 1, palette bits $70 when a tile's attribute byte has bits $1C, else $60); *w / *h its size |
| `titlelogo_tiles` | (a helper) | The title logo's graphics: n bytes at unpackbuf to a byte per pixel (palette bits added) from titlelogobuf+$40, 32 source bytes at a time; the bytes to send |
| `LoadTitleLogo` | `$9DBB14` | The centre ice logo of team a in the title's mode 7 map: the map buffer sent, then the logo's map centred at ($42, $40), its graphics unpacked, converted and sent |
| `TitleBlankSprites` | `$9DB891` | Title: an empty sprite frame, then a 35-frame wait |
| `BuildTitlePath` | `$9DBC8B` | The title logo path, $500 steps: each step's mode 7 scale, x offset and y scroll |
| `DrawLicenceNHL` | `$9DB983` | The licence screen: the NHL shield, the NHLPA logo and the NHL line at y $B0 |
| `DrawLicenceNHLPA` | `$9DB9B4` | The licence screen with the NHL line moved up and "Players Association" under it |
| `PrintCopyright` | `$9DB954` | The two copyright lines |
| `RollCredits` | `$9DBA13` | The credits, a step when its wait is over: the title and name scroll up, hold, then the next name (or title, or the end) |
| `TitleScreen_5` | `$9DB430` | Title: the next team's logo when the logo path has ended |
| `TitleScreen_6` | `$9DB44B` | Title: the logo along its path (frame hook): $C00 a frame, $500 steps, then the other side; the step's scale and offsets from BuildTitlePath's tables |
| `TitleScreen_7` | `$9DB5B6` | Title: the palettes, the licence and logo tiles, the maps and the sprites |
| `TitleScreen_9` | `$9DB73F` | Title: sprite a alone for 4 frames |
| `TitleScreen` | `$9DB118` | The title screen with its music: the logo zooms in, the licence screens with the team logos along their path, then the credits, until they end or a button |
| `title_irq_next` | (a helper) | (the title's next IRQ hook, its bank stored as a word: the byte after IRQHook's, seed's low byte, cleared) |
| `TitleScreen_framehook` | `$9DB4B2` | Title frame hook: a press of TitleSkipMask ends the title (titleskip), then the top band's display registers, .irq1 at line $46 and the logo along its path |
| `TitleScreen_irq1` | `$9DB50C` | Title IRQ hooks: the WH3 window edge, then .irq2; the mode 7 band's matrix and scroll, then .irq3; BG1 on the main screen |
| `TitleScreen_irq2` | `$9DB52F` | The title's second IRQ: mode 7 for the logo band (its matrix, centre and scroll), then .irq3 at line $B0 |

### `game/screen/video.c` (88)

| Function | Original | What it does |
|---|---|---|
| `hat_move` | (a helper) | One frame of hat k's flight: it moves; in the air it falls (hatvz, its own: launching clears only the first hat's); on the ice it slides, slowing by a sixteenth, until it stops |
| `updatesound` | `$9C9930` | The crowd noise voice's gain and pitch from Crowdlevel (at most $380) |
| `showclock` | `$9FA078` | Print the game clock (m:ss, at 3, 24; on the horizontal rink at 13, 11, but not in period 4) unless a replay is playing. |
| `addframe` | `$9BBFB9` | Draw sprite frame sprframe (at most $3D0) at sprx / spry, flipped by sprhflip / sprvflip |
| `setffo` | `$9FBC5F` | Draw the markers of the pad-controlled players (PadControlObj's slots 6 to 0): the slot's star, and below it the player's number in the small font and, but in the shootout, his pad's button label |
| `showzam` | `$9FA5AD` | Draw the zamboni moving across the horizontal rink during the intermission (x from the frames since vcountmark, less the borrow c leaves: the caller's carry) |
| `checkfo` | `$9FB1FD` | Draw the big faceoff sprites (both centres and the ref) while a faceoff is set up on the vertical rink |
| `setsortcords` | `$9CA054` | Draw the bench cheer, then the sorted objects (OOlist) in display order, hiding ones off the screen: penalized players over the boards, flips from the attributes (none for the bench and boards frames), palette $2200 for attribute bit 0, the cup over the cup frames |
| `crowd_frame` | (a helper) | (showcrowd's .7: crowd slot x shows step crowdanimstep of its sequence p0 (of animation p1) for its time (3 * word 6): the frame's tiles sent to VRAM (word 0 the tiles a row, word 4 the first, word 2 the VRAM row); unless the sequence has ended. |
| `puck_in` | (a helper) | (showcrowd's test of a slot's puck window: the puck above height $1A, its screen height (3 puckz + pucky) within [lo, lo + 8) and puckx within [xlo, xhi)) |
| `showcrowd` | `$9B9FAF` | Animate the crowd sprites (slots $22 down to 0, from $28 in Montreal when its fans or song are on): a running sequence steps on when its time runs out (at its end: a cheer goes back to the fans' animation, the others to $0C, then now and then $0E, then off); an idle slot starts by chance when the crowd is loud enough for it, the goal-end slots $22 / $20 when the puck flies into their window; the hat trick hats first. |
| `setvideo_t1` | `$9FBB50` | setvideo's .t1: the arrow r0 (3 frames: up / down, the side, a corner) of the player pad y controls when he is off the screen, at the screen's edge toward him |
| `setvideo_t3` | `$9FBB01` | setvideo's .t3: the off-screen arrows of the five pads (not in the pregame pause or with sflags bit 11) |
| `setvideo` | `$9FBAAA` | Once per game frame: build the sprite table for the next vblank (the rink, faceoff sprites, the off-screen arrows, the sorted objects, pad markers, clock, zamboni, crowd, referee, the rink rows, on the horizontal rink the team blocks) and queue the changed text |
| `SetFrameHook` | `$80855E` | Run the routine at hook (bank << 16 \| address) as the frame hook of the next NMI |
| `ClearNMIHook` | `$808569` | Set the NMI hook back to the empty routine |
| `WaitFrame` | `$808583` | Wait for the next frame (FrameCount changes: the NMI increments it) |
| `SetIRQ` | `$808520` | Turn on an IRQ at line y with the routine at hook (bank << 16 \| address): NMITIMEN $81 while the hook is changed, then a frame later $B1 (NMI, pads and the H/V IRQ) |
| `IRQOff` | `$808507` | Turn the IRQ off (NMI and pad reading on) and the IRQ hook to IRQHook_Off |
| `BrightnessHook` | `$9C9971` | NMI hook: the fade brightness to INIDISP unless the screen is blanked |
| `IRQ_StatsScroll1` | `$9BCE21` | Stats screen split 1 (at irqline1): BG1VOFS = irqvscroll1, then IRQ_StatsScroll2 at irqline2; scrollirqdone set for the frame |
| `IRQ_StatsScroll2` | `$9BCE44` | Stats screen split 2 (at irqline2): BG1VOFS = irqvscroll2, then IRQ_StatsScroll3 at irqline3 |
| `IRQ_StatsScroll3` | `$9BCE61` | Stats screen split 3 (at irqline3): BG1VOFS = irqvscroll3, then IRQ_StatsScroll1 again (the NMI sets VTIME back to irqline1) |
| `IRQ_BandStart` | `$9DCD0F` | Top of the band (at irqline1): BG1 scrolled to ($FFFC, $FFFB), then IRQ_BandEnd at irqline2 |
| `IRQ_BandEnd` | `$9DCD34` | Bottom of the band (at irqline2): BG1 scrolled back to (0, $FFFF), then IRQ_BandStart at irqline1 |
| `IRQ_BG1Scroll` | `$9B93C1` | A two-part BG1 split: BG1VOFS = BG3VScroll, then IRQ_BG1Scroll_t1 at line $30 |
| `IRQ_BG1Scroll_t1` | `$9B93DE` | IRQ_BG1Scroll's second part (at line $30): BG1VOFS back to $FFFF, then IRQ_BG1Scroll again |
| `SetColours` | `$9DE0BD` | Copy x colours from [p0] into the colour buffer from colour y |
| `WaitFrames` | `$80858F` | Wait x frames, y counting the rounds of 65536 (WaitFrames_Inc adds 1 to y first) |
| `WaitFadeOut` | `$9F9708` | Wait until the fade has reached black |
| `SetupBandIRQ` | `$9DCCF5` | The band IRQ pair (lines a and x) that shifts the BG1 scroll |
| `SetupScrollIRQ` | `$9BCDF8` | The split scroll of a stats screen: the IRQ lines a, x, y, the three scrolls $FFFF |
| `ResetPPURegs` | `$8086D5` | Reset the scroll, window and colour math registers and their copies |
| `SpriteFrameEnd` | `$80AFDE` | Move the unused OAM entries off screen (Y = $E1), swap the OAM buffers and set OAMReady |
| `QueueVRAMDMA` | `$808950` | Add a DMA of x bytes from [p0] to video memory at y to the NMI's transfer list, in pieces of $1000 bytes at most; the VRAM address after it is returned and *p0 moves past the source (its low word: the second add takes the first's carry) |
| `ScreenOn` | `$8086BD` | Full brightness (fading in unless already there), no forced blank |
| `BlankScreen` | `$808684` | Wait for a frame, set the default hooks, turn the IRQ off, force blank (a frame later HDMA off) and drop the DMA queue |
| `DMAToVRAM` | `$80891F` | DMA x bytes from [p0] to video memory at y (negative: where it is, r3 as the caller left it): at once with the screen blanked, else queued for the NMI (QueueVRAMDMA, which moves *p0). |
| `DMAToVRAMHigh` | `$80899B` | DMA x bytes from [p0] to the high bytes of video memory at y. |
| `FillVRAM` | `$8087EA` | Fill x bytes of video memory at y (negative: where it is, r3 as the caller left it) with the word a, the DMA's fixed source at $0016 (r3+2, which holds a): at once with the screen blanked, else queued in $1000-byte pieces (type 9). |
| `WaitDMAQueue` | `$80859A` | Wait until the DMA queue is empty (the NMI moves its head up to the tail) |
| `PasteMap` | `$808E2D` | Copy map [*p0] (width, height, then the entries from +6) into the map at [*p1] (r7 entries a row), column x, row y, adding a to each entry; *p0 and *p1 end past the last row (their low words, with the carries of the original's adds) |
| `PasteMapPart` | `$808E7C` | Copy a part of map [*p0] (its width at +0, entries from +6): r4 entries by r4+2 rows from column r6, row r6+2, into the map at [*p1] (r7 entries a row) at column x, row y, adding a to each entry; *p0 and *p1 end past the last row (their low words, with the original's carries) |
| `PasteMapPart_t1` | `$808ED2` | Fill y bytes at [p1] with the byte a, from the top down (y = 1 goes round the 64 KB) |
| `InitSprites` | `$80AEE1` | Start the sprites: sprite tile 0 at VRAM a, x OAM entries reserved, y tiles (at least $1C0, else $1A0, in 32s) for the 16x16 blocks; the block and tile allocators empty, the frame cache clear, the OAM buffers reset and an empty frame ended |
| `FillVRAMSplit` | `$808862` | Fill x VRAM words at y with the word a, the low bytes (from $0016) then the high bytes ($0017): at once with the screen blanked, else queued in pairs of $1000-byte pieces (types $FD, $FC). |
| `cache_add` | (a helper) | (record the frame in the tile cache: its entry number by frame number, address, bank, number, allocation, first tile; the entry's VRAM address, tile * 16 + SpriteVRAM with the carry of the last shift) |
| `dma_entry` | (a helper) | (LoadFrameTiles: DMA queue entry x, type 1 (DMAP), len bytes from bank:src; the caller stores the VRAM address at +6) |
| `split_run` | (a helper) | (split run x of length len at need: the rest a free run after it, which points back at x, and the run after the rest at the rest; the second add takes the first's carry) |
| `LoadFrameTiles` | `$80B3E1` | Allocate sprite VRAM for the frame at [p0] (p0 the direct page's 4 bytes: the cache keeps the word at p0+2), queue the DMA of its tiles (one or two rows: PieceX / PieceY bytes) and add a tile cache entry: its entry * 2, $FFFF when there is no room or all 128 entries are in use, x when the frame has no pieces. |
| `FindCachedFrame` | `$80B3CE` | The tile cache entry (* 2) of the frame at [p0], searching y entries from x; else LoadFrameTiles |
| `GetFrameTiles` | `$80B01E` | The first VRAM tile of the frame at [p0] (its entry by frame number with FrameByNumber, else by address), loaded now if needed (x as the original's X there), its use mark refreshed; FrameTilesFail and $FFFF when there is no room |
| `piece_entry` | (a helper) | (one OAM entry for the current piece at (px, py), if on screen: the tile word (piece tile + first tile, xor r3) and the high table's size and X bits; mode: 0 plain, 1 mirrored, 2 flipped (x mirrored too when r3 has $4000)) |
| `draw_pieces` | (a helper) | (DrawSprite_draw's piece loop: an OAM entry for each of the r0 pieces of the frame at [p0], 7 bytes each from +$16 (X, Y, tile, size), while sprites are left) |
| `DrawSprite_draw` | `$80B0B0` | Draw the frame at [p0] at (r5, r5h), r3 xored into each tile word (bit 15: flipped, the pieces' positions mirrored, DrawSpritePieces; bit 14: mirrored); x is the original's X at the tile cache lookup |
| `DrawSprite` | `$80B08D` | Draw frame `frame` of the sprite set at [p1] (p1 the direct page's 4 bytes) at (x, y), a xored into the tile and attribute word |
| `FreeCacheEntry_go` | `$80B77B` | Free tile cache entry x: the last entry moves into its slot (its frame pointed at it), and its VRAM run is given back, joined with a free run after it (blocks: in the same row of 8) and with a free run before it |
| `FreeCacheEntry` | `$80B772` | Free entry x of frame number y (its FrameCacheOf slot cleared); _A: the entry in a's high byte |
| `FreeFrameTiles` | `$80B719` | Free the VRAM tiles of the frame at [p0] (by its number with FrameByNumber); not cached: nothing. |
| `FreeFrameTiles_t1` | `$80B724` | (unused entries) FreeFrameTiles for frame a of the sprite set at [p1]; the sum of the free block run lengths, walking from index SpriteBlockTiles |
| `FreeFrameTiles_t2` | `$80B8AE` | (unused entry) The sum of the free block run lengths, walking BlockRuns from SpriteBlockTiles (after a free run the step is the sum so far, as the original) |
| `SpriteFrameStart` | `$80AF7B` | Start filling OAMDrawBuf after the reserved entries (the high table's shift from the byte offset, as the original); age the tile cache's use marks and free the entries not drawn since |
| `DMAToCGRAM` | `$808723` | DMA x colours from [p0] to CGRAM at colour y: at once with the screen blanked (none for x = 0; y is a byte there, CGADD), else left for the NMI (CGDMALen etc.; the colour word as y is) |
| `DMAToCGRAM_t3` | `$808766` | (unused entry) x colours from colour y of [p0] into [p1]: a = 0 fills them with y * 2 (the index, as the original leaves it in A), $0100 copies them, else scales each component by the byte a / 256 (the multiplier) |
| `PulseColour` | `$9D96A9` | Store at y:x the colour of this moment (vcount) of a 60-frame pulse: white to $7E10 and back, each component interpolated over 30 frames (the divides' remainders as the divider leaves them) |
| `map_addr` | (a helper) | (the screen map in VRAM: the word address of column c, row r) |
| `map_dma_now` | (a helper) | (MapToVRAM with the screen blanked: channel 1's size n, source src and VMADD vram for a row; the caller starts the DMA) |
| `MapToVRAM` | `$808ADF` | Copy map [p0] (width, height, then the entries from +6) into the screen map in VRAM at column x, row y, a row at a time (a row crossing column 32 in two, the rest on the next screen); at once with the screen blanked, else queued |
| `MapPartToVRAM` | `$808BED` | Copy a part of map [p0] (its width at +0): r4 entries by r4+2 rows from column r7, row r7+2, into the screen map at column x (32 or more: the next screen), row y; at once or queued as MapToVRAM |
| `MapPartToVRAMOffset` | `$808A11` | The same into VRAM through VMDATA with the screen blanked (it waits for that), adding a and ORing r3 into each entry |
| `IRQ_Fast` | `$8084E9` | The IRQ: acknowledge TIMEUP and run the hook at IRQHook |
| `LinearToPlanar` | `$9B85C2` | Convert linear 4bpp tiles at p0 (r0 bytes: r0 / 4 + 1 rows of 8 pixels, one row more than the bytes) to SNES planar into p1 (a 16-bit address in p0's bank): each row's two words, a nibble per pixel (a zero word skips its 4 pixels), into the row's plane words at +0 / +$10 of its tile |
| `Mode7MapToVRAM` | `$808D43` | Copy byte map [p0] (width, height, then the bytes from +6) into the mode 7 map at column x, row y (128 bytes a row), a row at a time: at once with the screen blanked (VMAIN 0, the low bytes), else queued (type $FB) |
| `Mode7MapToVRAM_t1` | `$808DED` | (unused entry) paste r6+2 bytes of [p0] into the map at [p1] (r7 entries a row) at column x, row y as words + a; a 0 byte ends the string, the rest of the count filled with $20 + a |
| `NMI` | `$808147` | The NMI vector: VBlank, then NMI_FrameHookReturn unless NMISkip was set when it came |
| `VBlank` | `$80814B` | The NMI: nothing while NMISkip is set (the 16-bit RDNMI read takes TIMEUP too), else the DMA budget for the frame, the NMI hook (NMIHookNext taken when set), then NMI_HookReturn. |
| `nmi_dma` | (a helper) | (a DMA of channel 0 for a queue entry: its source and size from the entry, to VRAM at +6, if the budget left covers cost + its size; 0 when not) |
| `NMI_HookReturn` | `$808186` | The NMI after its hook: the IRQ timers, INIDISP, the six BG scroll registers, the colour DMA left by DMAToCGRAM, then the DMA queue while the frame's budget lasts (entry types: a DMAP value; $FE $FF one or two words written through VMDATA; $FD / $FC low / high byte fills; $FB low bytes; the rest high bytes), then NMI_SendOAM (NMI_Tick when the budget ran out) |
| `NMI_SendOAM` | `$8083A5` | The NMI: the queue head at x; the sprite table to OAM when it is ready and the budget has $26E left; then NMI_Tick |
| `NMI_Tick` | `$8083DD` | The NMI's frame work: vcount (with the 50 Hz fraction's carry), a step of the fade (in after the fadestep delay, out with $FFFF; not on the frames STAT77 bit 7 is set), FrameCount (unless held), then the frame hook (FrameHookNext taken when set) |
| `NMI_FrameHookReturn` | `$808469` | The NMI after the frame hook: the pads (Multi5Bios; b the accumulator's high byte the hook leaves), SoundUpdate, the sound stream, NMISkip cleared |
| `DrawSpritePieces_cacheat` | `$80B389` | (unused entry) The tile cache entry * 2 of the frame at [p0], found by its number with FrameByNumber, else by its address; LoadFrameTiles when it is not cached |
| `DrawSpritePieces_cacheframe` | `$80B394` | (unused entry) DrawSpritePieces_cacheat for frame a of the sprite set at [p1] (DrawFrameNum = a) |
| `updatescroll` | `$9FBC41` | Set the rink scroll from the camera position (horizontal mode has its own) |
| `find3d` | `$9BC10F` | Project rink position d0 / d1 (height d2) to the screen: d0 = x, d1 = y ($4E20 off the screen) |
| `updatescroll_rows` | `$9FBFA0` | Queue the rink map rows that the vertical scroll brought on screen (at most $1D: the whole screen) into the BG2 ring of rows, and set BG2's scroll |
| `UpdateHatTrick` | `$9BA1A1` | While the hat trick is on (not on the horizontal rink or in a replay): the ten hats, each launched at random (a chance in 32 a frame) on an arc onto the ice, animated, moved timestep frames and drawn with its shadow |
| `DrawHorTeams` | `$9F9E24` | Horizontal rink: both team blocks (home at column $14, visitors at 3, row 2) and their palette rows |

## Menus: setup, the options, rosters and lines

### `game/menus/optsetup.c` (24)

| Function | Original | What it does |
|---|---|---|
| `opt_row` | (a helper) | (a row's entry in .13, by play mode: 1 shown, $FFFF the playoff name, 0 not shown; asl's carry added as adc does) |
| `setoptions_7` | `$9DA8B8` | Setup: change the option of row d7 by d2 (a team row, or the mode, also restarts the matchup pictures), within .14's choices (team rows: $1A in the playoff modes), wrapping round; a2 left at OptPlayMode |
| `setoptions_9` | `$9DA921` | Setup: d3 + d2, below d5 (signed) to d4 - 1, from d4 on to d5 |
| `setoptions_10` | `$9DA973` | Setup: the option rows: the teams (setteams) when they changed, then each row the play mode shows, its name and choice (the row at d7 highlighted, the team rows by name) |
| `setoptions_t1xxxxxxxxxxx` | `$9DAC22` | Setup: team x's matchup player's short name; the home one's (x = 0) copied to $0A5C |
| `setoptions_t2xxx` | `$9DABC1` | Setup: the next matchup (0-5, then 6 the logos): the sides showing a team turn to the matchup player's picture and name |
| `setoptions_11` | `$9DAA83` | Setup, each pass: the team pictures (a blank box when the mode has no such team row); every $78 passes the next matchup, its title, team blocks or player names and pictures |
| `setoptions_12` | `$9DAC4F` | Setup: the cursor row d7 moved by d0 to the next row the play mode shows (kept at the ends) |
| `setoptions` | `$9DA5CA` | The game setup screen: up and down pick a row, left and right change it, Start goes on (the password screen in play mode 1, new playoffs in 2 and 3), then the player select screen; left alone long enough it starts the demo with random teams. |
| `LoadTeamLines` | `$9B9334` | Copy team a2's lines ($38 bytes at its team data's offset +6) into its RAM lines, then without line changes the 8 bytes at offset +$0C over their start |
| `PrintSelectLeft` | `$9BCA7A` | Print the Select Left hint on the password screen, d4 columns on |
| `ClearBigText` | `$9DAF5B` | Clear the big text letter tiles (bigtextbuf+1, $300 bytes) and part of its map (screenmap+$28C) |
| `InitSetupChars` | `$9DE074` | The setup screen's tiles: the blank tile $200, the small font after it (unpacked, its VRAM cleared first), the frame tiles after the font and the score tiles 4 further |
| `SendBigText` | `$9DB0AB` | Send the big text map (screenmap from +6: a 32 x 2 part at row printy) and the letter tiles to VRAM |
| `Multi5MouseMsg` | `$9DB058` | The MultiPlayer 5 warnings: the mouse, the Super Scope, the socket |
| `Multi5ScopeMsg` | `$9DB005` | The warning that the MultiPlayer 5 is not designed for use with the Super Scope |
| `Multi5SocketMsg` | `$9DAFB2` | The warning that the MultiPlayer 5 must be connected to controller socket 2 |
| `LoadMatchupPhotos` | `$9DCC36` | Unpack the six matchup players' photos of team x (0 home, 2 visitors) into the $7F photo buffer (visitors at $2D00, home at $5100, each at its MatchupPhotoOffsets place), unless that team's are already there |
| `DrawSelectTeam` | `$9DAE6D` | Team d1 on the player select screen at printx: its palette (colours $70 for the home team, unless both teams are the same, else $60), its block map (with redrawblocks) and its city name in the big font |
| `bigsprite_letter` | (a helper) | The width of letter c (and its frame, f; -1 for a space) in the big sprite font |
| `PrintTeamSprites_go` | `$9C9479` | The letter loop of PrintTeamSprites: the width of string s (to its 0; a space first has none; '.' is 3 wide), letterx moved to centre it (r3h 0 or a2 negative), or to end at $F8 (a2 0), then each letter drawn (palette $3800, on the horizontal blocks $3E00 home / $3C00 visitors) |
| `PrintTeamSprites` | `$9C943E` | Draw the place name of team d1, or string [a1] (made to end at its length) when d1 is negative, as big letter sprites at x / y |
| `RedrawPlayerSelect` | `$9DAF84` | Clear the message rows and redraw the PlayerSelectAlt title and the team pictures |
| `PlayerSelect` | `$9DA0D5` | The player select screen after the game setup: each pad moves its icon under the visitors or home team (cont1team-cont5team), Start goes on. |

### `game/menus/setup.c` (114)

| Function | Original | What it does |
|---|---|---|
| `clearTeamStats` | `$9FC6B2` | Clear the team stats ($1AC bytes from tmPwrGoals, the goalie choices kept) and both penalty box lists, then set up the teams |
| `setteams` | `$9FC6D8` | Point both team structures at their team: sort start, team number and data pointer |
| `SprSort` | `$9CA19B` | Sort the objects by screen position for drawing: Ylist the y (x on the horizontal rink) of the 16 objects, then a bubble sort of OOlist (object * 2 bytes), OOlistpos following |
| `SetTeamColors` | `$9FB91A` | Copy both teams' uniform palettes (rows 12 and 2 home, 13 and 3 visitors) and the player marker palette (row 10) into palbuffer |
| `setplayercolors` | `$9FB901` | Set the team colours and send the whole palette to CGRAM |
| `resetplstuff` | `$9FD252` | Reset the six players of each team: no animation in progress, the ready (goalie) or glide animation, no impact, collision block, backwards or lock |
| `ScrollDisplayUp` | `$9ECD9D` | The next line of the scouting report (column 0); once 8 lines are on the screen the 8 text rows move up a row |
| `setupice_reset` | `$9FB8B4` | Reset the rink state (black screen, scroll 0, no pause, the sort objects; the players' goals flipped for the other direction of play), set the colours and fade in |
| `unpack` | (a helper) | (setupice: unpack tile set p0 to unpackbuf; its size in bytes, *tiles its tiles (32 bytes) and *c the carry the last shift leaves, for the adc after it) |
| `send_tiles` | (a helper) | (setupice: the unpacked tiles to VRAM, after the last set) |
| `setupice` | `$9FB69E` | Set up the rink screen once the screen has faded out: the video mode, the rink palette (rows 0-7 and 8-15), the game sound driver, and every tile set unpacked into VRAM one after the other, each set's first tile kept (the rink, the centre ice logo, faceoff, rink logo, refs, a blank tile, the small font and Framer box remapped, the big font, the team blocks); then setupice_reset |
| `DrawTeamBlocks` | `$9FBE08` | Draw team d1's block at printx / printy: its name as letter sprites (8 pixels left on a player card, 4 right and down there and in the pause menu), then, unless the horizontal rink's blocks are only being redrawn, its colours (palette 7 for the home team, 6 for the visitors; both when the teams are the same) and its logo map (2 rows of 9 tiles) in the text map |
| `bracket_tile` | (a helper) | (PlayoffScreen's .t1xxxxx / .7x: one 8 x 8 tile of brackettiles at x (its two plane bytes a row), row 0 from pattern 1 and the others from r0: bits 0-3 give the four bytes, set the values in v, clear in c) |
| `bracket_entry` | (a helper) | (PlayoffScreen's .9: the team block colour entry x (bytes of colourbuf: its tile and attribute, twice; the attribute's palette from y) and the bracket tiles x * 16 (from bits 1-4 of y) and the next) |
| `playoff_column` | (a helper) | (PlayoffScreen: send the columns r7 of the two maps (screenmap 28 rows, screenmap2 32) that the scroll brought on) |
| `PlayoffScreen` | `$9D8BBF` | The playoff tree screen (in the playoffs only; the playoff song from the second round): the menu background, the bracket tiles and colours for every team, the teams of the round's tree in big letters with their blocks, the bracket lines, the series scores, the Stanley Cup and the password; then it scrolls with the pad (left / right, to the round's stops, within its limits) until start, the team colour pulsing |
| `scout_skip_line` | (a helper) | (UpdateScoutingDisplay_skip: script a2 past its next carriage return) |
| `UpdateScoutingDisplay_para` | `$9EC9E2` | Print the report's next word or % field from PregameText a1 + d0 at the line column (DispAttribCtr), the next one $0A frames later |
| `UpdateScoutingDisplay_skip` | `$9EC9CB` | Skip the rest of script a2's line and d0 more, then print from there |
| `UpdateScoutingDisplay_next` | `$9EC9D7` | Skip d0 lines of the script a2 (from PregameText a1), then print from there |
| `UpdateScoutingDisplay_scroll` | `$9EC9B1` | Start paragraph d0 of PregameText a1 on a new line (a blank line before it once text is up) |
| `UpdateScoutingDisplay` | `$9EC95C` | Every frame of the scouting report: when its timer runs out (at once with asv negative), the next word of the paragraph, or the next paragraph of the list TestList; at the list's end the timer stops and the screen stays 480 frames |
| `GameOver` | `$9F8DB6` | After a game: the password and the playoff screen; in a playoff series with a user game the next game (from its scouting report unless sflags3 bit 2 was set), else back to the start (Opening2). |
| `PeriodOver` | `$9F8CB1` | End of a period: fade out, the menu or playoff music, the teams turned around (players out of the game back on the bench), the next period number (tied after the third: overtime, again in a playoff game), the intermission; then game over (GameOver) or the next period (StartPer). |
| `restart_go` | (a helper) | Start over: the menu music (unless it is playing), vertical mode, the option screens, then the game (StartGame) or the shootout, after which it starts over again. |
| `Opening2` | `$9F82D2` | Back to the option screens from anywhere (the end of a game or the demo, exit game): out of the playoffs, then restart_go on a fresh stack |
| `Opening` | `$9F8283` | The cold start after Reset_Fast: the intro, bank $7F cleared, the password and default options, the title screen, then the option screens (Opening2, which the original falls into) |
| `GetShifter` | `$9D98B5` | The games of playoff round playoffRoundIdx less one (*d1) and its first win bit (*d2) |
| `SuperAdd` | `$9BB4A3` | The password bits (128) += d0 (clears the carry) |
| `SuperMult` | `$9CA4CF` | The password bits *= d0 (shift and add through mulres, $DD-$EC; clears the carry) |
| `SuperDiv` | `$9CA41B` | The password bits /= d0, the remainder returned (in d0). |
| `DefaultMenus` | `$9D9C97` | The default menu choices at power on: the 8 option words from OptPlayMode on, not a demo |
| `igs_mark` | (a helper) | mark team d0 taken in the bit sets (teamtaken nonzero if it was) |
| `InitializeGameStructures` | `$9FC79F` | Random team pairs for the 8 other games (gst1 / gst2), none of them HomeTeam or VisTeam and no team twice |
| `box` | `$9FA669` | Erase the goal box: its position string (boxPos) printed, then 19 x 9 blank tiles |
| `PushBits` | `$9BCDB3` | The password bits = the password bits * d1 + d0 |
| `rpb_value` | (a helper) | the next value of range n out of the password bits, added to the checksum in d7 (.sd) |
| `ReadPassBits` | `$9BCC41` | The playoff state back out of the password bits (last written first): the series wins, WinBits, pojoy, potreeteam, playoffRoundIdx, bosgames, postarts ($FFFF when the checksum is wrong). |
| `wpb_value` | (a helper) | value d0 of range d1 into the password bits, added to the checksum in d7 (.t1) |
| `WritePassBits` | `$9BC623` | The playoff state into the password bits, with a checksum (the running sum kept in d7) |
| `FigureJoy` | `$9D995D` | Which team each controller plays: in the playoffs the series of the user's team gives HomeTeam, VisTeam, OptTeam2 and gamenum and the pads in use their team (the same table word for each); in the demo, play modes 0 and 4 only the other games |
| `MakeTree` | `$9D9783` | Build the playoff tree: the 16 seats of tree postarts into potree, the winner of each pair (by WinBits) after them, then this round's series (gst1 / gst2, home first in the 2-3-2 rounds of bosgames 2, 3 and 5) with their scores cleared, and FigureJoy |
| `NewPO` | `$9D98D3` | New playoffs with team 1 (25 at most): a random tree (from the next of a random number on) that holds the team, its seat potreeteam and the tree postarts, the first round; bosgames 7 for single games (play mode 2), else 0 and the series wins cleared; then the tree |
| `rg_winbits` | (a helper) | the round's winners d3 (a bit a series, set for the second team) into WinBits at the round's first bit d2, and the next round |
| `ResolveGames` | `$9F94AE` | Record the played playoff game (gamenum, the score tmscore) and the series wins: single games (bosgames 7) decide the round at once; in best of seven the round ends after the seventh game or, before round 3, once the user's series is decided (the other series played out at random and the password bits of the moment kept in tpassbits). |
| `GetPlayerName` | `$9FC3F8` | mesarea = the number and name of player TempPlOffset (bit 15 set for the visitors); a1 = mesarea |
| `GetPlayerNumInitial` | `$9FC4A1` | mesarea = the number, first initial and last name of player TempPlOffset; a1 past it |
| `EncodePW` | `$9BC75E` | After a playoff game: work out the winners and the new password and tree; when the user won it all or is out (a one-player first round swaps to the other team instead) the password is hidden and the play mode is 2 (3 unless the rounds are single games) |
| `ShowInjuryBox` | `$9EC35F` | The injury box: its frame, the injured player (TempPlOffset) and whether he is out for the period or the game (bit 14, cleared here) |
| `ShowGoalBox` | `$9FA697` | The goal box: both line change boxes closed, then the last goal of the goal log (ScoreSum): the frame (taller with each assist), "GOAL" (on a skater's third goal "HAT TRICK", with the hats for the home team and a longer stoppage), the scorer and the assists |
| `InitScoutingDisplay` | `$9FC87C` | Reset the scouting report text scroll; a0 = TestList |
| `ResetPassWord` | `$9BCDE2` | Blank the password |
| `StartHatTrick` | `$9BA189` | Start the hat trick sprites: clear the ten slots and set hattrick |
| `GetPlayerNamePointer` | `$9FC732` | The roster record of player d0 of team a2 (a0): past the team header, then d0 records of (length word + 8) bytes |
| `FinalizeTextBuffer` | `$9ECE06` | End the string built in mesarea: its length word = a1 - mesarea; a1 = mesarea |
| `BigLetterPlayoff` | `$9D9C53` | RenderBigText's letter hooks: letter tile bigtextchar (two rows) at printx / printy of screenmap2 (PlayoffScreen) or screenmap (DrawSelectTeam) |
| `BigLetterSelect` | `$9D9C76` | RenderBigText's letter hook on the team select screen: tile bigtextchar (two rows) in screenmap |
| `GetPlayerInitial` | `$9FC576` | mesarea = the first initial and last name of player TempPlOffset (bit 15: a visitor); a1 = mesarea |
| `bigtext_or` | (a helper) | OR a letter's 8 pixel rows (from r1, shifted right r3) into tile t of bigtextbuf |
| `bigtext_put` | (a helper) | Store a letter's 8 pixel rows (from r1, shifted left r3+2 + 1) into tile t of bigtextbuf: the last one |
| `bigtext_rows` | (a helper) | The pixel rows of letter r2's half (0 top: its BigLetterMap entry; 1: the one $36 bytes on) |
| `bigtext_width` | (a helper) | The width of the string in pixels (a space at the start counts nothing) |
| `jml_vector` | (a helper) | RAM's jml trampoline: the routine at jmlvector+1 (RenderBigText's letter hooks) |
| `RenderBigText` | `$9D9A5C` | Draw string p0 in the big proportional font: letter tiles into bigtextbuf from tile bigtextchar, centred in $24 pixels from printx, the jmlvector hook placing each letter on the map. |
| `BitstoPW` | `$9BCD30` | Turn the password bits into the password characters (the remainders by PWrange, the last first) |
| `PWtoBits` | `$9BCD69` | Turn the password characters into the password bits |
| `PlacePasswordCursor` | `$9BCAB8` | Move the password cursor to character d5: rows of 6 from the record's position |
| `PrintPasswordHelp` | `$9BCBD8` | Print the password screen keys |
| `getname` | `$9FC372` | mesarea = the uniform number and name of roster player d0 of team a2; a1 = mesarea |
| `FormatPlayerNumInitial` | `$9FC4D0` | mesarea = the number, first initial and last name of roster player d0 of team a2 (a word written at a time); a1 = mesarea |
| `PWprint` | `$9BC6C9` | Print the password (PassWord's characters through PasstoText), unless it is hidden (bit 7) |
| `ClearRosterName` | `$9FC5A6` | Erase the scouting report name line of side a2 |
| `star_above` | (a helper) | Score lo / hi above the star's (signed words, high first) |
| `star_add` | (a helper) | A 32-bit score (low word, high word) plus v |
| `star_score` | (a helper) | Star score of player x (+$1A for the visitors): his team's goal difference (a 32-bit value), then for a goalie with enough shots against (40 / the period length's low byte: the divider's operands swapped) $7D00 below 3 goals per 100 shots and $124F8 more for none; for a skater $B71B0 for the overtime winner, $2AF8 a goal, $2774 an assist and $0A a shot. |
| `choose_stars` | (a helper) | Choose the three stars: every player's star score, the best three kept in order (the overtime winner read from the word before the score summary, the faceoff spot: a quirk of the original) |
| `ShowThreeStars` | `$9C9BAB` | Stars of the Game: a frame with the three stars' teams and names |
| `BuildRinkMap` | `$9BC1D5` | Unpack the rink map to rinkmapbuf and paste the home team's centre ice logo map (CenterIceTable+4: its width, height, then the tiles from +6) into its middle, a row higher when 6 or more rows tall |
| `LoadCenterIce` | `$9BC17C` | The home team's centre ice logo tiles: unpacked, made planar after the source, sent to VRAM after the rink tiles (faceoffvrcset the tile after them), then the rink map |
| `hl_decide` | (a helper) | A tied highlight game decided at random (a goal to one side), then over (period 5) |
| `hl_restore` | (a helper) | (SaveRegsStartHL2: the game state back from hlsavebuf after an other game's highlight, as hl_save left it) |
| `hl_save` | (a helper) | The highlight copy of the game state: OOlist+$10 .. |
| `hl_team` | (a helper) | Team a2 for a highlight: full energy, its players on the ice, no touches |
| `StartHL2` | `$9F8E1C` | The highlight of playoff game a0 / 2 (d1): its ticker box and "Highlight from" for 3 seconds (Start skips it, and the rest), then the game played from a faceoff with the cpu on both sides for a few seconds of clock (a button ends it, a tied game given a goal), the score kept, the menu screen and music back. |
| `BadPasswordPrompt` | `$9BCAF8` | Bad password: wait for a button, then offer retry or exit. |
| `print_pw_char` | (a helper) | (the password character d5 at the cursor printed in the one-character string PWCharStr) |
| `PasswordRedraw` | `$9BC946` | The password entry: the cursor on character d4 of the password (A / B next and back, the d-pad picks its character d5), Start checks it (BadPasswordPrompt). |
| `PasswordScreen` | `$9BC7FB` | The password entry screen: the characters in five rows of six, the cursor on the password's first character; returns d2 as PasswordRedraw leaves it |
| `IntroSequence_33` | `$9FFAB4` |  |
| `IntroSequence_31` | `$9FF9F5` | Intro: x + 1 colours of CGRAM from colour y filled with colour a |
| `IntroSequence_2` | `$9FF4D8` | Intro: the palettes, the work buffer cleared, the logo's tiles and map unpacked and sent to VRAM in the mode 7 layout, the map block copy that .28 sends, the sprites unpacked |
| `IntroSequence_22` | `$9FF89C` | Intro: the Mode 7 centre (X $200, Y $200) and the matrix $100 0 0 $100 |
| `IntroSequence_27` | `$9FF8F3` | Intro: the 18 by 10 block of map bytes in the buffer's copy (row stride 128 words from word $37), copied from the map at p0 (from its byte 6) or, d1 = 0, cleared |
| `IntroSequence_32` | `$9FFA08` | Intro: $20 bytes from [p0] to the buffer (the palette a fade works on) |
| `IntroSequence_29` | `$9FF975` | Intro: the palette at p0 into the buffer, the palette job's source the buffer, 16 colours |
| `IntroSequence_30` | `$9FF992` | Intro: one fade step of the buffer's 16 colours, each component +1 up to 31; 1 when all are white |
| `IntroSequence_3` | `$9FF5E3` | Intro: the start state: BG1 scroll $180 / $190 (job $10 sends it), next state 3, d6 the timeout (vcount + $2D0), d7 the earliest end (vcount + $F0), the Mode 7 registers. |
| `IntroSequence_6` | `$9FF64D` | Intro states (.4's jump table), each 0 to go on, 1 at the end. |
| `IntroSequence_7` | `$9FF676` | State 1: the palette toward white, up to 3 steps a frame; once white, reloaded from p0, then state d5 |
| `IntroSequence_8` | `$9FF6A9` | State 2: wait until vcount reaches d6+2, then state d5 |
| `intro_logo` | (a helper) | (states 3 and 5: the logo's map block into the buffer from scale 1, zoomed in (state 0), then state d5) |
| `IntroSequence_9` | `$9FF6BA` | State 3: sound 0 and the E logo |
| `intro_sprites` | (a helper) | (states 4 and 6: sprites 0 to n - 1 at ($38, $2F), then state next) |
| `IntroSequence_13` | `$9FF78B` | State 7: the map block cleared, the sports palette job (16 colours from $20), zoomed (state 0), then state $B |
| `IntroSequence_14` | `$9FF7C5` | State 8: the text palette job (16 colours from $10), then state $D |
| `intro_fade` | (a helper) | (states 9 and 10: the palette at p0 for colours from tmp0 into the buffer, faded in (state 1), then state d5) |
| `IntroSequence_18` | `$9FF863` | State 12: 30 frames in state 2, then state 8 |
| `IntroSequence_19` | `$9FF878` | State 13: the end once at full brightness and d7 is reached, with a button or at the d6 timeout |
| `IntroSequence_4` | `$9FF613` | Intro: one step of the state machine (d4 = d4+2), unless the frame hook still has jobs; 1 at the end |
| `IntroSequence` | `$9FF436` | The power-on sequence before the title (the publisher's logo in mode 7): its own sample set and song, until it ends or a button. |
| `ConvertByteToDigits` | `$9FC5BE` | The two digit (BCD) number d0 as two characters at [a1], a blank for a leading zero; returns a1 past them |
| `defaultsprites_0` | `$9CA227` | Clear the objects ($76E bytes from Xpos) and set the 16 sort objects up from their start table (9 words each: x, y, height, frame, attribute, collision width and height, assignment, flags), the sort lists in order; then sort them |
| `IntroSequence_framehook` | `$9FFA17` | The intro's frame hook: the jobs of introHookFlags, $80 the map block to VRAM (.28), else $08 the palette tmp2/3 (colour tmp0, tmp1 of them; with $200 the second one, introPal2Src); $10 the BG1 scroll; $40 the mode 7 matrix A and D |
| `IntroSequence_t1xxxxxxxxxxxx` | `$9FF94F` | Intro: DMA channel 0, a bytes from p0 to VRAM address x |

### `game/menus/stats.c` (63)

| Function | Original | What it does |
|---|---|---|
| `team_counts` | (a helper) | the byte of team a2's data that counts its forwards and defensemen (team data byte 3 past the offset at +8) |
| `GetPlayerCount` | `$9FC93D` | The players on team a2's roster (records up to the terminator word 2) |
| `AttribRating` | `$9F98B1` | Roster column tail: d0 as a number of 4 digits, then the text AttribFightingPos; returns a1 past the number |
| `AttribWeight` | `$9F9901` | Roster column: the weight in pounds (8 * rating + $8C), then the text AttribWeightPos |
| `AttribEnergy` | `$9F9870` | Roster column: player d0's energy of team a2 as a percentage (energy / $28, $1000 with automatic line changes, at most 100) |
| `AttribStatus` | `$9F97DA` | Roster column: player d0 of team a2's status from tmpdst: a negative word a state (StatusTextTbl 0-3, larger ones 1), bit 12 state 3, otherwise a penalty time and string 4 (5 with bit 14) |
| `GetNameandAttrib` | `$9F9745` | Print player d0 of team a2's number and name, then at column $19 ($1A out of the pause menu) the attribute d4 through attribjmp (from 3 on the rating of mask d4+2 through CalcAttrib, hot / cold included, r2 its weights) |
| `player_rating` | (a helper) | the rating column d1 (AttribMenuSkater, or AttribMenuGoalie for a goalie: curplayer below the goalie count) of player curplayer of team a2 through CalcAttrib |
| `swap_next_slot` | (a helper) | (SetLineSlot's .5: swap the line slots at a3 and a3 + 1) |
| `SetLineSlot` | `$9B8C7A` | Put player a in the cursor slot of the selected line (SelectedPlayerIdx) of team a2; if he is already in a nearby slot of the line (searched by how far the cursor is from the line's end), the old occupant of the cursor slot moves there |
| `ShowRosterLine` | `$9D9ECB` | Print the roster line SelectedPlayerIdx of team a2 (0 the goalies) with the attribute column DispAttribCtr (stepped back past the menu's end): the line name and column title, the area below cleared, then each player of the line with GetNameandAttrib under its position name (or goalie mark) |
| `SetupScreen` | `$9DD9AD` | Common start of the stats screens: the old screen faded out, the team logo sprites unpacked, the video registers, the small font, Framer, team block, nine tile and outline tiles one after the other (each set's first tile kept), the menu background (its 7 x 7 logo box part and the whole map in textmap2) and its palettes; for setupvis $21 and up (less $21) the setup background over it; colour 0 black, the sprite palettes, the text map cleared, the sprites started and the matchup pictures drawn |
| `line_list_redraw` | (a helper) | (SelectLinePlayer's .10: the column title (DispAttribCtr of SkaterAttribColumns, AttribMenuGoalie for the goalies, stepped back past the end), "PLAYER", the arrows when the list scrolls, and the list of team a2) |
| `SelectLinePlayer` | `$9B8D86` | Line editor: pick a player of team a2 for the cursor slot (lineslots - linecursor) of line SelectedPlayerIdx from the list of defensemen (the last slots) or forwards, 9 rows at a time, starting at the player in the slot; left / right change the attribute column. |
| `TeamRosterScreen` | `$9D9D26` | The team roster screen (pause menu) of the pause pad's team: the title, frames, team block and the current line with an attribute column; then the loop (TeamRosterLoop): start leaves (RosterRtl), Y switches team (SwapRosterTeam, then TeamRosterRedraw), left / right step the column (MoveRosterColumn), up / down the line (0-7) |
| `InitStatsOrder` | `$9C8B54` | statsorder[i] = i for the rows of the list (one at least) |
| `SetPlayerStatsRange` | `$9C8826` | The first and last row of the player stats list of team a2: its goalies (statcolumn 0), else its skaters (after the goalies); curitem the first |
| `WaitButtonRelease` | `$9B865F` | Wait until the pause pad's buttons are up (the pads are read by the interrupt) |
| `GetTeamLongName` | `$9BC5AB` | The full name of team a (a1): its TeamList city, or the long New York names for teams $0D / $0E |
| `PrintToStatsPage` | `$9EC894` | Print string a1 at printx / printy into the stats page buffer (32 wide) through SmallFontMap. |
| `ComparePlayerStats` | `$9C8C22` | Compare stat column statcolumn of the players at order slots x and y of team a2: statstemp = 1 when y's is greater (signed bytes; goals + assists as words), else 0 |
| `SwapLineSlots` | `$9B8D4D` | Swap the line slot bytes at d0+2 and d0+4 (in a3's bank); a3 is left at the second |
| `PrintTeamAbbrev` | `$9B9B35` | Print the abbreviation of the home team (d3 = 0) or the visitors on the stats page, then two rows down |
| `ClearPlayerStatBraces` | `$9C8858` | Erase the braces around the player-stat rows shown (from curitem, listrows of them), before a scroll |
| `ClearOtherScoreBraces` | `$9BC4F3` | Erase the braces around the other scores shown (three rows a game from listtop), before a scroll |
| `PlayerInitialName` | `$9FC427` | mesarea = the first initial, a period and the last name of roster player d0 of team a2 (a word written at a time, so a byte past each); a1 = mesarea |
| `PrintOtherScoreTeam` | `$9BC601` | Other scores: team a's full name at column 7 and its score d0 at column $14 of the stats page |
| `ClearLineArea_erase` | `$9B89E6` | Line editor: erase the menu area (32 x 17) |
| `PrintScrollHint` | `$9B9A5D` | Print the scroll hint under a summary list |
| `PrintOtherScore` | `$9BC543` | Other scores: game curitem's two teams with their scores, and its period label (one on for a decided period 4) |
| `FillOtherScores` | `$9BC49A` | Other scores: from game r0 on, every game's line but the user's (counted in otherskip), up to listend |
| `LoadPictureMap` | `$9DDF73` | Unpack the picture map to picboxmap and add the second font's tile base and palette 3 to each word |
| `DrawStatsList` | `$9C88A0` | Player stats: the header (skaters' or goalies') and its rows to VRAM, then the visible rows of the stats page from row curitem |
| `DrawTeamScreen` | `$9B91A2` | The line editor's background: the top cleared, the frame outline, the background map into text map 2 |
| `penalty_line` | (a helper) | One penalty summary line pair at printy: the period, the time, the team, the player; below, the minutes and the penalty's name |
| `FillPenaltyPage` | `$9B9917` | The penalty summary page in the stats page buffer: a line pair for each of the listend logged penalties |
| `scoring_line` | (a helper) | One scoring summary line set at printy from the goal entry at a3: period, time, team, strength (SH / PP by the two kinds of each), the scorer and below him the assists |
| `FillScoringPage` | `$9B95B0` | The scoring summary page in the stats page buffer: a line set per goal of the score summary |
| `stat_cell` | (a helper) | One stat, a column's palette, at printx |
| `DrawStatsList_rows` | `$9C8934` | The player stats page from row listtop to listend, in statsorder: skaters' goals, assists, points, shots and penalty minutes (the sorted column highlighted), or goalies' goals against, shots and save percentage |
| `SortPlayerStats` | `$9C8B69` | Player stats of team a2: the column's name, the rows from listtop sorted by column statcolumn (ComparePlayerStats), the page filled |
| `Pausemode_go` | `$9FE3BA` | (the goalie: right / A / B the next one, after the last none; left the one before, before the first none, from none the last) |
| `TimeoutMenu` | `$9FE431` | Timeout: A / B use one of the team's timeouts (if it has one left), its players rested, "timeout" and the team's name shown a moment |
| `ExitStatsScreen` | `$9FE79F` | Back to the rink after a pause screen: the rink map, its tiles, the centre ice logo, the faceoff, logo and referee tiles, the fonts (remapped), the score and team block tiles to VRAM, the screen registers and the palette; then the horizontal rink, the sprites, and the screen faded in |
| `summary_loop` | (a helper) | The penalty / scoring summary list: Start leaves, up / down scroll it a row (six rows drawn while the IRQ split slides, then five), the arrows redrawn (the penalties' pad read with key repeat) |
| `summary_screen` | (a helper) | A summary screen: its title in letter sprites, the colours, the heading, the list (with the scroll hint from 6 entries) |
| `otherscores_loop` | (a helper) | The other scores list: its braces (none above the first game shown, or the user's game first), each frame; Start leaves, up / down scroll a game (the user's game, skipped in the list, stepped over) |
| `ShowScores` | `$9BC25A` | The other scores screen: the other games of the round (the user's skipped), five at a time |
| `PlayerStatsScreen` | `$9C8583` | The player stats screen of the pausing team (Y the other team): up / down scroll the rows, left / right the sorted column (column 0: the goalies), Start leaves |
| `crowdmeter_lit` | (a helper) | The crowd meter's lit part: the first crowdmeterlit words of each row of map [p0] (width and height, then the words from +6) plus a, into the map at [p1] (r7 words a row) at column x, row y |
| `crowdmeter_bar` | (a helper) | The crowd meter for level a dB: (a - 50) / 4 cells lit, past 18 the broken meter |
| `crowdmeter_values` | (a helper) | The crowd meter's three values (current, peak, average; curitem's highlighted) and its bar for it |
| `CrowdMeterScreen` | `$9BA428` | The crowd meter screen: the current, peak and average levels, up / down choose the one the meter shows, Start leaves |
| `DrawTeamScreen_title` | `$9B8970` | The line editor's title: the background redrawn (drawlinesback, then the band IRQ line back), the label, team a2's block and the pictures |
| `DrawEditLinesTop` | `$9B890F` | The line editor's top: cleared, the title, the frame outline |
| `PrintLinePlayers` | `$9B8B60` | The players of the selected line (SelectedPlayerIdx; 0: the goalies) of team a2, a row a slot with its position name, the cursor slot highlighted (its player the home matchup picture); then the title |
| `PrintLineNames` | `$9B8810` | The selected line's name (the line change names, or the best line names without line changes), then its players |
| `PrintLinePlayerList` | `$9B9249` | The line editor's pick list: pickrows players from picktop + 1 (the cursor's highlighted), each with attribute d4 of mask (goalies' from the goalie list) |
| `LineEditorExit` | `$9B8A15` | The line editor's menu (Start): Return to game (A = $FFFF) or Restore default lines (team a2's lines loaded again, A = 0) |
| `LineEditor` | `$9B869B` | The line editor of the pausing team: left / right the line (with line changes on; lines 6 and 7 have four slots), up / down the slot, A / B pick a player for it, Start the menu |
| `GetPauseTeam` | `$9DE0E9` | a2: the team of the controller that owns the pause menu (2 the away team, else 0) |
| `GetGoalieCount` | `$9ED300` | The number of goalies of team a2: the nibbles of its goalie odds word (byte-swapped) until the rest is zero. |
| `PrintGameInfo` | `$9FA586` | One ticker line at column px (NewTicker3's, on the stack): team d0's name, its score d1 at px + $17 |

## Sound: the sound CPU's uploads and the sound effects

### `game/sound/sound.c` (52)

| Function | Original | What it does |
|---|---|---|
| `SPCSendSample` | `$8095C6` | Sound driver command 3: send the n (X) bytes of the sample at [p0] to the sound CPU, loop point (the next sample slot) y, each byte through APUIO2 with its count on APUIO1 echoed back |
| `StreamSfx` | `$9C9A7F` | Ask for streamed sound effect a (1-6): a different stream playing (3 and up) has its sound stopped, and the stream starts over; a different stream 1 or 2 keeps playing |
| `SPCTitleToMenu` | `$9C9891` | Turn the title sample set into the menu set: the last three samples dropped (command 4), the four of the menu set after its first 12 sent |
| `StartSoundVoice` | `$80A697` | Put the note of track x / 2 (CurTrack; TrackInst its entry, EventCode the note, EventData1 the velocity, EventData2 the length) on a free voice with its pitch, volumes and envelope, sent by SeqCmd07; nothing without an entry, a voice, or a length for two special entries. |
| `UpdateSfxVoices` | `$80AC7B` | Each frame, each voice in use: a sound effect voice (track $10 and up) counted down (UpdateVoice); its age counted up, and between the entry's vibrato start and end ages, when the vibrato counter runs out, its rate, depth and speed stepped and the vibrato offset moved (only then added to the pitch sent); then by its state at the start: 2 the pitch (SeqCmd0B), 4 the gain lowered by entry byte $10 (stopped below 0), others gain and pitch |
| `PlaySoundEntry` | `$80A226` | The start shared by QueueSound and PlaySound (InstPtr the entry, track $10, X = $20): the note, its length and the track's bend range and volumes from the entry (the bend range and bend stored at the track's offset $20 as an index: past their lists), the velocity SfxVelocity's high byte; StartSoundVoice's accumulator |
| `PlaySound` | `$80A200` | Play sound a (a byte) at once on the sound effect track: no replay record, no test of a voice playing it already |
| `QueueSound` | `$80A17D` | Start sound a on a sound effect voice (none when a voice plays it already); outside a replay it is kept in recsfx for the replay record, the one before moved to recsfx2. |
| `sfx` | `$9C9AD0` | Play sound a with the velocity vx / vy: the loudness from their high bytes, (x^2 + y^2) / 8 (at most $70) into SfxVelocity's high byte, the pitch the entry's times $C0-$FF (seed) / 256; a voice QueueSound just started in state 2 takes the pitch, state 3. |
| `PuckSfx` | `$9C9ABC` | Play sound a with half the puck's velocity (Y as sfx leaves it) |
| `CollisionSfx` | `$9E9B0E` | Play the collision sound ($0F) with the doubled velocity of the faster of objects x and y, so meant: the second's length is compared with its own shorter side (ApproxLength leaves it in r3 over the first length), so it is y's but for a length that wraps |
| `stream_sfx` | (a helper) | Each frame: the streamed sound effect streamsfx (but while the sound is locked or the menu set is loaded; with no game clock and no song, cursong cleared). |
| `UpdateStreamSfx` | `$9C9981` | Each frame: the streamed sound effect's sample sent to the sound CPU a block at a time, then played (stream_sfx) |
| `sample_upload` | (a helper) | (a sample set upload: the screen black, sndbank set, the sample area emptied (command 1), the player reset on the sound table, then the n bytes of 4-byte long pointers of the list sent) |
| `SPCGameUpload` | `$9C98DF` | Switch the sound driver to the in-game sample set, unless it is loaded |
| `SPCIntroUpload` | `$9C97E9` | The intro sample set (marked as the game set), unless the game set's mark is there; no team song |
| `SPCTitleUpload` | `$9C983D` | The title sample set (the menu table, marked as the game set), unless the game set's mark is there; no team song |
| `SPCMenuUpload` | `$9C978E` | The menu sample set, unless it is loaded; no team song, no streamed sample loaded either way |
| `spc_send` | (a helper) | (the SPCCmd routines' send: cmd to APUIO0, then wait for the driver to echo it back) |
| `spc_ack` | (a helper) | Sound driver commands $07 (voice x: volume, sample, ADSR from [p0]), $08 (main volume, pitch modulation, noise, echo, FLG), $09 (echo), $0A (the echo filter: 8 bytes), each in two or three sends |
| `SPCSetVolume` | `$8097B7` | Command $0E: the volume of voice x, a the left (APUIO2) and y the right (APUIO3) |
| `SPCStatus` | `$80959A` | Command $02: the sample count (the result, a byte) and the free sound RAM pointer (*x, a word: APUIO1 and APUIO2) |
| `SeqCmd0B` | `$80A4FD` | Sequencer: command $0B, the pitch SendPitch of voice y (APUIO2 and APUIO3) |
| `SeqCmd08Init` | `$80A3F5` | Sequencer: clear the command $08 values (after keying every voice off), then send command $08 (the volume and pitch modulation) and $82 (noise and echo voices) |
| `SeqCmd08Init_send` | `$80A40F` | Send command $08 (SoundVolume's high byte as both sides' main volume, PitchModVoices) and $82 (NoiseVoices, EchoVoices) as they stand: SeqCmd08Init's second half, entered too by SeqCmd08, the volume fade and the events that change one of them |
| `ClearTracks` | `$80A3D8` | Clear the track pointers and waits of tracks y up to x (8-bit counters: y = x goes all round) |
| `StopVoice` | `$80ABC7` | Stop voice y (x = 2 * y): a streamed effect's stream state cleared, the voice free, owned by no track, and a gain $9F sent |
| `ReadVarLen` | `$80A568` | A variable length number (7 bits a byte, top bit = more) from the track at [EventPtr] (its low word steps; the 16-bit reads take the byte after too, the 8-bit X only the byte) |
| `SPCSetVoice` | `$809649` | Command $07: voice x's volume, sample and ADSR, 5 bytes from [p0] in two sends ($07, $81) |
| `SPCSetFIR` | `$80970C` | Command $0A: the 8 echo filter coefficients from [p0], in three sends ($0A, $84, $85) |
| `SPCDownload` | `$8098BF` | Sound driver command $11: x bytes of sound RAM from address y to [p0], a byte a handshake on APUIO1 |
| `SoundVoicesOff` | `$80A3C1` | Release every voice (the sound locked meanwhile) |
| `MusicStop` | `$80A349` | Stop the song: release the voices of its tracks (below $10) and clear its tracks |
| `StopSound` | `$80A2A8` | Stop every voice that plays sound a (its entry through SoundTable; after the first stop the comparisons are a byte: StopVoice returns with 8-bit registers) |
| `ReleaseVoice` | `$80ABF7` | Put voice y (x = 2 * y) into its release: state 4, its gain word from the instrument's byte $15, and a release gain by the instrument's byte $11 (a voice in state 1 just stops) |
| `UpdateVoice` | `$80A51C` | Voice y (x = 2 * y): count its length down; at 0 release it unless it is releasing already or its track holds the sustain pedal (a negative length: no limit) |
| `SoundInit` | `$80A380` | Silence everything and reset the player: every voice released, the sound RAM from SongUnused on cleared, SoundVolume $5000, the tracks cleared, command $08 sent, TempoTick $12 |
| `MusicStart` | `$80A2E0` | Start the song at [p0]: the tempo from its second byte (DivX8U leaves the 16-bit accumulator, so TempoTick's store takes the byte after it too, and EventPtr+2 the word at p0+2: p0 here is the direct page's 4 bytes), the tracks from its offset list at +8, the track count from +6 |
| `SeqCmd07` | `$80A465` | Sequencer: command $07 (the voice's volume and the instrument's sample and ADSR, bytes $14-$16), $06 (its pitch) and, unless ADSR is on (byte $15 bit 7), $0C (its start gain); Y = the voice |
| `SeqCmd08` | `$80ADE9` | Sequencer: command $08 (volume, pitch modulation, noise and echo voices) and, with echo voices, $09 (echo volume, feedback, buffer at $E0, delay) |
| `FindFreeVoice` | `$80A610` | A voice for the sound at InstPtr (byte 0: the voices it may use, $2A: its priority): a free one from after the last used (VoiceRotor), else the usable one of no higher priority with the highest state, then age (the ages are read from voice 7 down whatever voice is tested; the 16-bit best age takes InstVolL and VoiceVolL), which is cut (command $0C; $FF none) |
| `SPCStreamStart` | `$8097D9` | Sound driver command $0F: start streaming a sample of x bytes from [StreamSrc] with loop point y, sending its first block ($8A bytes, or $73 when STAT78's version bits are 0); a byte a handshake on APUIO1. |
| `SPCStreamNext` | `$80983E` | Sound driver command $10: the next block of the stream ($8A bytes, or $73 when STAT78 bit 4 is clear; the rest when it is the last, 0 = 65536); the bytes still left |
| `SPCUpload` | `$80AE7B` | Send the stream at [p0] through the sound CPU's boot loader: blocks of length, sound RAM address and data, a length of 0 ending it with the start address (each block's kick value the last index + 4, never 0) |
| `ReadTrackEvent` | `$80A58E` | Read the next event of the track at CurTrackPtr: its delta (ReadVarLen, not kept), its code into EventCode and its data into EventData1 / EventData2 by the code (a note: the key and its length; $E7 skips a block) |
| `UpdateVoices` | `$80A549` | Update each voice in use that belongs to a music track (below $10), CurVoice and VoiceOfs the voice |
| `PlayTeamSong` | `$9C9B5F` | Play song slot a of the home team (TeamSongs), or a random DefaultSongs song when it has none, unless a song plays; cursong = a + 1 |
| `StepTrack` | `$80A86F` | Track t: count its wait down, or play its events until one gives a wait (notes start voices, $D9 and up are commands: $D9/$DA stop the song, $E2/$DB loop (from the pointer the track's events started at), $DD tempo, $DE volume, $DF controllers, $E5 pitch bend (the pitches it sets take the tuning of the entry at InstPtr, whichever it is), $DC program change); returns CurTrack |
| `SequencerTick` | `$80AC4A` | The tempo: TempoAcc + $40, and while it reaches TempoTick (less it each time) a tick: the music voices updated and, unless the music is paused, every track stepped (from the last; none when the song stops, and no more ticks without tracks) |
| `SoundUpdate` | `$80AD9D` | Each frame, unless the sound is locked: the fade (SoundVolume less SoundFadeSpeed, the sound reset below 0, else the volume sent), the sound effect voices and the song's ticks (the pause's mute of the volume is undone at once) |
| `SPCUploadBlock` | `$809906` | SPCSendSample of the sample at [p0]: its loop point and length (big-endian words), then the BRR blocks from +4 (the sound locked meanwhile) |
| `spc_cmd5` | (a helper) | (SPCSetMaster and SPCSetEcho: 5 bytes from [p0] in two sends, cmd with the first three, cmd2 with the last two) |

## System: the 65816's arithmetic, interrupts, DMA, sprites, decompression, the sound sequencer, passwords and data

### `game/system/decompress.c` (9)

| Function | Original | What it does |
|---|---|---|
| `DecompressRLE` | `$80C2F3` | Run-length data at y (data bank db; its header word at +3 is $0101 for bytes, else byte pairs) to WRAM through the port: a negative count n copies ~n + 1 literals, a positive one repeats the next byte (pair) n + 1 times, 0 ends |
| `lz_lit` | (a helper) | LZ data at [p0] (its commands from +5) to WRAM through the port (LZOutAddr follows it, from p1; copies come from the WRAM already written, at LZCopySrc). |
| `lz_copy` | (a helper) | (DecompressLZ: a copy of n bytes to the port from the output already written, at LZCopySrc) |
| `lz_from` | (a helper) | (DecompressLZ: a copy's distance into LZDistance, and its source LZCopySrc dist + 1 bytes back from LZOutAddr) |
| `DecompressLZ` | `$80B8DB` | Decompress's format $FB10: the LZ data at [p0] to WRAM at p1, command by command until the end run |
| `DecompressPairs` | `$80BA63` | Decompress's format $FB46: y the data after the format word (from y+3 the escape code, the pair count and the pairs: code, first, second; then the packed bytes), expanded to WRAM |
| `hf_number` | (a helper) | (.149 / .150: a number of the stream: 1 and 2 bits (0-3), or k zeros, a 1 and k + 3 bits over the base of .151; HuffBits the window after it) |
| `DecompressHuffman` | `$80BBB3` | Huffman data at (p0) + 5 in data bank db to WRAM at p1 through the port. |
| `Decompress` | `$80C373` | Unpack the data at [p0] to WRAM at [p1] (through the WRAM port, once the DMA queue is empty) by its format word: $FB10 LZ, $FB46 pairs, $FB30 Huffman, $FB7A run-length; the unpacked size from the header (its big-endian word at +3), or 1 for an unknown format |

### `game/system/main.c` (2)

| Function | Original | What it does |
|---|---|---|
| `CheckPAL` | `$9B8000` | The frame time step for a 50 Hz console (STAT78 bit 4; the 16-bit read takes APUIO0 too) |
| `Reset_Fast` | `$80802A` | The reset: the PPU's registers cleared (the mode 7 matrix the identity, colour math $30 / $E0), VRAM cleared, the sound driver uploaded, all of WRAM cleared, the RAM tags, sprite masks and hooks set, the sprites and the sound started, the screen black, then the game (Opening) |

### `game/system/middle.c` (71)

| Function | Original | What it does |
|---|---|---|
| `randomd0A` | `$9BB370` | A random number from 0 to limit-1 |
| `Mod100` | `$9F9E15` | d0 mod 100 |
| `randomd0s` | `$9BB3B0` | randomas with the range in d0: a random number from -d0 to d0-1 (clears the carry) |
| `PushNumberWidth` | `$9FC63B` | d0 as d1 digits, right aligned with leading blanks, in PushWidthBuf (more digits run on to the left); returns a1, the string, and in *a the last quotient (0) |
| `PushNumberWrap` | `$9FC623` | PushNumberWidth, with d0 taken mod 100 when the width d1 is 2 |
| `FramerRow` | `$9D804F` | One row of a frame into the text map at printx / printy: the corner [p0], d0 - 2 of the middle tile [p0+2], the corner [p0+4], each plus tmp0; printy steps down. |
| `framer_box` | (a helper) | a d0 by d1 frame of the map at map (+6 the top row, +12 the middle, +18 the bottom) with tile base tmp0, the changed range of the text map kept (Framer and its kin) |
| `FramerNineTile` | `$9D8091` | Framer with the nine-tile box (FramerNineTileMap) of tile set ninetilechars |
| `FramerOutline` | `$9D80DD` | Framer with the outline box (FramerOutlineMap) of tile set outlinechars |
| `ReadPads` | `$9B8017` | Once per frame: each pad's direction (PadDirection of its pad bits), changed buttons and new presses |
| `ProcessInputWithRepeat` | `$9B852C` | Key repeat: held buttons d3 repeat (into d1) after a delay; a change d2 restarts it. |
| `Readjoy1` | `$9B84F5` | Pad 1: d0 = direction, d1 = new presses, d2 = changed buttons, d3 = buttons held |
| `WeightedRandomSelect` | `$9D9CD2` | A random index 0 ... |
| `remap` | `$9B854B` | Send d0 tiles from [a0] to video memory, each pixel's colour remapped through the table [a1]; returns a0 advanced past them |
| `Sroot` | `$9BB3BE` | The square root of the 32-bit d0 (a binary search, the range narrowed first) |
| `forceblack` | `$8086A9` | Turn the screen black: fade out (when lit), forced blank, PPU registers reset |
| `QueueTextDMA` | `$9FBF75` | Queue the changed part of the text map (textdirtylo to textdirtyhi) for transfer to video memory |
| `waitx` | `$9FA5EA` | Wait d0 + 1 frames (drawing each one) or until a pad with a team holds a button (ReadjoyA's d3); returns the pad read last (zero when no pad has a team: the direct page's d0-d3 as they were) |
| `Mul8XS` | `$808F5D` | 8 x 8 bits into 16 (A), X signed: A times X's magnitude, negated for a negative X (the carry set, or for a negative X set only when the product is 0) |
| `Mul8S` | `$808F7D` | 8 x 8 bits into 16 (A), both signed: the magnitudes multiplied, negated when the signs differ (two negatives leave the carry as it came) |
| `Mul16x8U` | `$808F98` | X (16 bits) x A (its low byte) into r2 ($08-$0B): unsigned; X signed; both signed. |
| `Mul16x8XS` | `$808FC9` | X x A (low byte) into r2, X signed: X's magnitude times A, negated for a negative X |
| `Mul16x8S` | `$808FEB` | X x A (low byte) into r2, both signed: the magnitudes multiplied, negated when the signs differ |
| `Mul16U` | `$809099` | A x X into r2 ($08-$0B): unsigned; X signed; both signed (the carry as for Mul16x8U) |
| `Mul16XS` | `$809080` | A x X into r2, X signed: A times X's magnitude, negated for a negative X |
| `Mul16S` | `$809062` | A x X into r2, both signed: the magnitudes multiplied, negated when the signs differ |
| `Mul16x8HiXS` | `$809032` | X x A (low byte) / 256, X signed: the product of X's magnitude / 256, negated for a negative X (so it rounds toward 0) |
| `Mul16x8HiS` | `$80904B` | X x A (low byte) / 256, both signed: the magnitudes' product / 256, negated when signs differ |
| `Div32U` | `$80923F` | r2 ($08-$0B) / r1 ($04) into r0, unsigned. |
| `Div32S` | `$8091FC` | r2 / r1 into r0, signed: the magnitudes divided, negated when the signs differ ($7FFF -> $8001) |
| `Div16S` | `$809197` | r2 (16 bits) / r1 into r0, signed: the magnitudes divided, negated when the signs differ ($7FFF for 0, $8001 when negated) |
| `DivX8S` | `$809327` | X / A (low byte) into A, X signed: X's magnitude divided by the unsigned byte, negated for a negative X (the collision and hat trick speeds: a distance over a length or a frame count) |
| `RandomWord` | `$808658` | A random word: the sum of two 16-bit shift-register generators (seeded $A5A5 / $B4B4 at 0) |
| `ApproxLength` | `$9CA554` | The approximate length of (x, y): the longer side plus a quarter of the shorter, or, when the shorter is more than half the longer, plus 3/8 of it (in 16-bit words, unsigned) |
| `ApproxLength_abs` | `$9CA568` | ApproxLength's second half, for x and y already positive (entered directly by the puck distance and the replay, which have the magnitudes) |
| `ReadjoyA` | `$9B84DC` | Pad n: direction, new presses, changed buttons, buttons held |
| `WaitSidePadsRelease` | `$9B866C` | Wait until every pad that has a side (cont1team) has no button down |
| `WaitPadsRelease` | `$9B8682` | Wait until every standard pad (low nibble 0) has no button down |
| `MemCopy` | `$808F2F` | Copy x bytes from [p0] to [p1], from the top down (an odd byte first); x = 2 or 3 goes round the whole 64 KB |
| `DisposePtrHandle` | `$809D69` | Free handle a unless it is a heap block (bank $7E or $7F) |
| `DerefHandle` | `$809F5B` | Handle a's address (low word) and bank (high word); 0 for a bad handle |
| `FindHandle` | `$809F83` | The handle of address a in bank x, 0 if none |
| `NewPtrHandle` | `$809D3C` | A handle for fixed memory at a in bank x; 0 if none is free |
| `HeapFreeBank` | `$809FCB` | sum plus the sizes of the free blocks (flags 0) of the heap chain in p0's bank from block y (a header: size, flags, next) |
| `WaitNoButtons` | `$8085FF` | Wait until no button is held on pad 1 |
| `GetNewPresses` | `$9B8472` | For each of the 5 pads: the buttons that changed, those newly pressed, then the buttons held |
| `MemFill` | `$808EDF` | Fill x bytes at [p1] with the word a (from the top down; x = 2 goes round the 64 KB); _t1 adds a to each word |
| `MemFill_t1` | `$808EEB` | MemFill's second entry: add the word a to each word of x bytes at [p1] (from the top down) |
| `MemOr` | `$808F03` | OR the word a into x bytes at [p1]; _t1 ANDs it |
| `MemOr_t1` | `$808F19` | MemOr's second entry: AND the word a into x bytes at [p1] |
| `Sin` | `$80954D` | sin / cos of a (1024 steps a circle, $0100 = 1), from the quarter-wave SineTable |
| `Div32x32U` | `$8091CA` | n / d into the result (32 bits), the remainder in *rem, unsigned, by shift and subtract (the divisor is first shifted until its top bit is set: d = 0 never ends) |
| `HeapInit` | `$809932` | The heaps: start them at x in bank $7E (at least $E68C) and y in bank $7F, one free block each (a header: size, flags, next, previous), all handles free |
| `HeapFree` | `$809FAA` | The free bytes of both heaps (X high word, A low) |
| `handle_block` | (a helper) | Handle a's block header (8 bytes before its address) in its heap's chain: *p0 the bank and the header's offset, 0 when not a heap handle or not in the chain |
| `LockHandle` | `$809CA4` | Set / clear the locked bit ($4000) of handle a's block (a free block is left alone; clearing keeps only the used bit) |
| `UnlockHandle` | `$809CF0` | Clear the locked bit of handle a's block, keeping only the used bit ($8000) |
| `MulFrac16` | `$809144` | a * y (y a byte) / 128 into the result and MulProd+1: the mode 7 multiplier, or with bit 6 of Mode7InUse a shift-and-add of 7 steps (the 16-bit MulPlier: its high byte shifts down too) |
| `MulFrac8` | `$8090ED` | c's low byte times its high byte (7 bits) / 128: the mode 7 multiplier, or with bit 6 of Mode7InUse a shift-and-add (each set bit adds the multiplicand and the carry, 1). |
| `HeapAllocBlock` | `$809A33` | The first free block of at least want bytes in the heap chain of bank (bank << 16) from header y, taken with flags \| $8000 (split when 16 bytes or more are left: the new free block's header after the data, its next the old next; the old next's previous is not updated); its data address, $FFFF for none |
| `PrintLetterSprites` | `$9C936C` | Draw string [a1] (its length + 2 at +0, the characters from +2) with the letter sprites at x, y: letters and digits, a space 6 pixels, other characters nothing |
| `DisposeHandle` | `$809AAB` | Free handle a and its block (8 bytes before its address in the heap of its bank), joined with a free block after it, then with a free block before it |
| `HeapCompactBank` | `$809BA5` | Join the free blocks of the chain in bank (bank << 16) from header y and move each used unlocked block after a free one down into it (its data by an ascending byte move), the free space moving up; the handle whose address word is the old data address points at the new one (whatever its bank) |
| `HeapCompact` | `$809B84` | HeapCompactBank for both banks |
| `NewHandle` | `$80999B` | A handle for a block of x bytes (a's bit 14: locked) in bank y ($7E or $7F; any other: either), compacting the heaps when it does not fit at first; 0 when no handle or no room |
| `SetHandleSize` | `$809D8E` | Resize handle a's block to x bytes: in place when it can (shrinking by 9 or more: the rest a free block, joined with a free block after it; growing into a free block after it), else in a new block (NewHandle, either bank, not locked) that the old data (its size + 1 bytes) is moved to, the old one freed and the handle kept |
| `Multi5ConnectCheck` | `$9B83BE` | Look for a multitap on each port: one when the first 8 signature bits are all 1 and the next 8 are not |
| `Multi5Init` | `$9B8358` | Check for the multitap, then clear the multitap pads and state |
| `mp5_sig` | (a helper) | Read the five pads: pads 1 and 2 from the auto read, pads 3 to 5 through the multitap on port 2 when mp5present is set (pads 4 and 5 only every 30 frames while nobody uses them); a port line change runs Multi5ConnectCheck for 16 frames; pads without the standard signature read $000F; with a warning every pad flashes $1000 / 0. |
| `Multi5Bios` | `$9B80AB` | The pads' read of each frame, run by the NMI after the frame hook: JoyPad[0-4], with the multitap's check and warnings |
| `PrintScrollArrows` | `$9B9AB2` | The scroll arrows beside a summary list (right while rows are below the five shown, left while rows are above), or blanks in their place; then the text map queued |

### `game/system/print.c` (11)

| Function | Original | What it does |
|---|---|---|
| `appstring` | `$9ECFA5` | Append string [a1] to string [a3] (each a length word, then the bytes; a word is stored per byte) |
| `PushNumber` | `$9FC5EB` | d0 as a decimal string ending at mesareaEnd, no leading zeros; returns a1, the string |
| `eraser` | `$9FBD9C` | Fill a d0 by d1 rectangle of the text map at printx / printy with tile d2 (plus the font set) |
| `PushTime` | `$9FA3D7` | The time d0 (seconds) as a minutes:seconds string ending at mesareaEnd; returns a1, the string |
| `print_control` | (a helper) | a control record (the negated byte n): palette and priority into printa, the font set, the map |
| `print_dirty` | (a helper) | the changed range of the text map: entry e (the map word index) |
| `print_pos` | (a helper) | the text map entry of the print position, and the carry the 65816 leaves (print's .1) |
| `print` | `$9EC7BE` | Print string [a1] (a length word, then bytes: bit 7 a control record followed by printx and printy, $40 a blank, others SmallFontMap tiles) into the text map at printx / printy; returns a1 advanced past it |
| `big_tile` | (a helper) | a big font tile: the map word at BigFontMap + off, its tile bases and printa |
| `printbig` | `$9ECE15` | Print string [a1] with the big font (2 by 2 tiles a character, narrow ones 1 wide; lower case as upper case, a space a blank block) at printx / printy |
| `Framer` | `$9D8000` | Draw a d0 by d1 frame (FramerMap: corners, edges, the fill) at printx / printy, the changed range of the text map kept |

### `game/system/rom.c` (2)

| Function | Original | What it does |
|---|---|---|
| `rom_set` | (a helper) | The cartridge from bytes (copied, a copier header skipped); 0 when shorter than one bank |
| `rom_load` | (a helper) | The cartridge from the ROM file at path (rom_set); 0 when unreadable |

### `game/system/snes.c` (3)

| Function | Original | What it does |
|---|---|---|
| `wait_r16` | (a helper) | (the loops that wait for an interrupt to change WRAM: read it afresh, and between two reads let the interrupt run) |
| `mset8` | (a helper) | a byte stored at a 24-bit address: WRAM, or the console's registers ($2100-$21FF, $4000-$43FF) in the banks that have them; the cartridge ignores it |
| `call_long` | (a helper) | Call the hook routine at addr (bank << 16 \| address), as the original's jml through the hook |
