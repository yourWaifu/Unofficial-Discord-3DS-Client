# Discord 3DS Client
This just a simple Discord client for the 3ds build using the [Sleepy Discord library](https://github.com/yourWaifu/sleepy-discord).

# Why
I noticed that a few Discord clients for consoles were using the Discord API incorrectly causing huge issues for those clients. So, I decided to make this as an example of how I think it should have been done. 

# How to use
First, you need a token to an account that you own, because Discord doesn't want people asking for passwords. Anyway, place this into a file called ``discord token.txt`` in the root of your sd card.

## Controls
| Function      | Button      |
|---------------|-------------|
| Next channel  | D-Pad Down  |
| Next server   | D-Pad Right |
| Load messages | Y           |
| Send Message  | A           |

# Dear Homebrew devs,

If you are a dev planning to make a Discord client as a cool homebrew app, please direct message Sleepy Flower Girl on the [Discord API server](https://discord.gg/discord-api) for help.

# How to build
If you haven't done so already, install devkitpro. You can find instructions to do this here http://wiki.gbatemp.net/wiki/3DS_Homebrew_Development#Install_devkitARM

Download and compile wslay from here https://github.com/Cruel/3ds_portlibs. Place lib and include files in ``devkitpro/portlibs/3ds``. Make sure this folder has all the library files you need including include files from ctrulib and citro3ds in your ``include`` folder (it's possible to get .h file not found errors if this isn't done).

Download and compile Sleepy Discord for the 3ds from here https://github.com/yourWaifu/sleepy-discord. Instructions for compiling for the 3ds is in ``buildtools/Readme.md``. Once that's done, place lib and include files in ``devkitpro/portlibs/3ds``.

Now you just need use make, and it should compile to a new folder ``output/3ds-arm``.
