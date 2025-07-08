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
The key to open the Overthrow menu is "U" (not "Y" due to key binding conflicts in Arma Reforger). 

## Development

### For New Developers
Complete development setup instructions are available in our comprehensive documentation:

📚 **[Development Documentation](https://wiki.armaoverthrow.com/en/development-documentation)**

Key resources:
- **[Environment Setup Guide](https://wiki.armaoverthrow.com/en/development-documentation/development-environment-setup)** - Complete setup walkthrough
- **[Architecture Guide](https://wiki.armaoverthrow.com/en/development-documentation/architecture)** - Understanding Overthrow's component system
- **[Coding Standards](https://wiki.armaoverthrow.com/en/development-documentation/coding-standards)** - EnforceScript conventions
- **[Development Workflow](https://wiki.armaoverthrow.com/en/development-documentation/development-workflow)** - Testing and compilation procedures

### Quick Start
1. Clone this repository using [GitHub Desktop](https://desktop.github.com/) (recommended)
2. Install "Arma Reforger Tools" from Steam (under Tools section)
3. Copy dependencies from `ArmaReforger/addons` to `ArmaReforgerWorkbench/addons`
4. Open `addon.gproj` in Workbench
5. Load `Overthrow/Worlds/MP/OVT_Campaign_Test.ent` for testing

**Tip**: Use the "test" world for development as it loads much faster than the full Eden map.

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

![Alt](https://repobeats.axiom.co/api/embed/49520d34e4c6206b4d0f149dd4b6c2fec786d606.svg "Repobeats analytics image")
