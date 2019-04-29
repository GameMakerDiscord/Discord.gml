# Discord.gml

This is a wrapper for [Discord](https://discordapp.com)'s Game SDK
for GameMaker: Studio / GameMaker Studio 2.

Written by [YellowAfterlife](https://yal.cc).  
Licensed under LGPL.

## What does it cover?

* [Activities/Rich Presence](https://discordapp.com/developers/docs/game-sdk/activities)
(for using RPC in non-Discord-Store builds, see [Dissonance](https://rousr.itch.io/dissonance))
* [Networking](https://discordapp.com/developers/docs/game-sdk/networking)
* [Matchmaking](https://discordapp.com/developers/docs/game-sdk/lobbies)
* [Overlay](https://discordapp.com/developers/docs/game-sdk/overlay)

For each of these, the extension does not necessarily cover _every_ function,
but enough of these to port a game that was previously using
[my Steamworks wrapper](https://github.com/YellowAfterlife/steamworks.gml).

## Compiling

1.	Extract discord_game_sdk.zip (which you get after you've been approved for Discord Store)
	to `Discord.gml/discord_game_sdk` directory.
2.	Copy `Discord.gml/discord_game_sdk/lib/x86/discord_game_sdk.dll` to `Discord.gml.gmx/extensions/Discord.gml/`
3.	Open and compile the included Visual Studio solution.

If you have [GmxGen](https://bitbucket.org/yal_cc/gmxgen) in your PATH, this will also auto-update
the extension to match any new functions/scripts/macros.

## FAQ

First, let's get a few things out of the way:
development of this extension was paid for by several high-profile GM projects
(Nidhogg, Knight Club, (unannounced)).

Later I contacted Discord to ask whether I could release my wrapper,
and whether they'd be willing to pay me to cover missing SDK functions and documentation,
but, unfortunately, I only got an OK for releasing it (and nothing at all on expansions), so...
you get what you get, I guess.

Now, to the actual FAQ:

*	**Is there documentation?**  
	As per above, not really.
	You can use "Show API" context menu option on the extension
	in [GMEdit](https://yellowafterlife.itch.io/gmedit) to view a list of functions
	and match them up to accordingly named functions in
	[official SDK documentation](https://discordapp.com/developers/docs/game-sdk).
*	**Are there samples?**  
	You can find a small sample project in the repository.  
	It is not extensively commented by any means, but it tests that functions are working correctly.  
	You will need to open a second Discord instance with a second Discord account using Sandboxie
	and open a second copy of the game in there to test matchmaking.
*	**Is there Mac/Linux support?**  
	Does Game SDK even support Mac/Linux? I'm not sure.
*	**Does it work in GMS2?**  
	It should work fine if you import the sample project into GMS2 and then "Add Existing"
	the extension from sample project to yours.