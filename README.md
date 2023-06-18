<div align="center">
<picture>  
  <img alt="Everon Life" width="400" src="https://github.com/ArmaOverthrow/Overthrow.Arma4/blob/main/UI/Textures/logo_overthrow.png?raw=true">
</picture>
<br/><br/>

[![Arma Reforger Workshop](https://img.shields.io/badge/Workshop-59B657D731E2A11D-blue?style=flat-square)](https://reforger.armaplatform.com/workshop/59B657D731E2A11D)
[![Contributors](https://img.shields.io/github/contributors/ArmaOverthrow/Overthrow.Arma4)](https://github.com/ArmaOverthrow/Overthrow.Arma4/graphs/contributors)
[![Discord](https://img.shields.io/discord/241062829963214848?label=discord)](https://discord.gg/j6CvmFfZ95)
[![License MIT](https://img.shields.io/badge/License-MIT-green)](https://opensource.org/licenses/MIT)
</div>

# Overthrow
A dynamic and persistent revolution platform for Arma Reforger (and eventually Arma 4)

## Installing
Overthrow is available in early access from the ARMA Reforger Workshop. Just search for "Overthrow".

## Playing
The key to open the Overthrow menu is not "Y", its now "U" (due to key binding conflicts in Arma Reforger). Read the [Wiki](https://github.com/ArmaOverthrow/Overthrow.Arma4/wiki) for more info.

## Development
If you want to setup a dev environment for Overthrow and help with development..

Start by cloning or downloading this Github repository. For Windows/MacOS we recommend using [Github Desktop](https://desktop.github.com/). 
 - [Installing Github Desktop](https://docs.github.com/en/desktop/installing-and-configuring-github-desktop/installing-and-authenticating-to-github-desktop/installing-github-desktop)
 - [Cloning a repository](https://docs.github.com/en/repositories/creating-and-managing-repositories/cloning-a-repository)
1. Install the "Arma Reforger Tools" in Steam. Find it by switching from "Games" to "Tools" in your Steam library.
1. Run the Arma Reforger Tools and click "Add Existing"
1. Navigate to the folder you cloned the Overthrow Github into and select `addon.gproj`
1. Before opening the project, copy the 3 dependancies into `My Documents/My Games/ArmaReforgerWorkbench/addons` from `My Documents/My Games/ArmaReforger/addons`
1. Double click on the Overthrow mod to open it in the Workbench
1. Double click on `Overthrow/Worlds/MP/OVT_Campaign_Test.ent` in the Enfusion Workbench resource browser
1. In the World Editor that opens, click on the Green Play button or press F5

We recommend using the "test" world for most things as it's much smaller and loads much faster, if you need to test things on a full map, open `Overthrow/Worlds/MP/OVT_Campaign_Eden.ent` instead

## Updating
Updates will be pushed to github often (sometimes multiple times a day). 

1. Sign up and/or sign in to Github. [See Github docs](https://docs.github.com/en/get-started/onboarding/getting-started-with-your-github-account)
1. "Watch" this repository in the top right to get email notifications when updates are made
1. Before updating Overthrow, exit game mode in World Editor
1. In Github Desktop, make sure "Current Repository" top left is this one
1. Click "Fetch Origin" and then "Pull Remote"
1. In the Reforger Script Editor, click on Build > Compile and Reload Scripts

## Contributing
Just [join our Discord](https://discord.gg/j6CvmFfZ95) and participate in the `#overthrow-reforger` channel. Report any bugs you find or join the discussion about features or suggest your own. If you are comfortable with github and Enfusion development then please do make pull requests or ask to help out.
