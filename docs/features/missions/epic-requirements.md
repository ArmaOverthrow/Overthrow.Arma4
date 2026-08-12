# Missions — Epic Requirements

"Missions" will be the epic that completely replaces our legacy "Jobs" system which has been one of the most problematic systems in Overthrow since inception. The job system will be completely nuked when missions are released.

What to keep from jobs:
We will keep the general "idea" of jobs, in that missions are config-driven and highly customizable, with entire missions able to be defined with no code using pieces, but expand on it greatly and write a new more solid backend + persistence framework for them.

However, the ethos here will be to redesign the system to be much more elegant. Do not look to the job system for architecture because it was bad to begin with. Only the idea of jobs as a declarative modular system was good.

Branching:
Missions should be able to branch, and as many times as needed. Players can make choices via their actions which can send a mission on a completely different path. 

Rewards and items:
Missions will be less "outcome-based". Missions can still be defined to give a reward on completion, including cash, xp or items. But rewards or mission-specific items can also be given at any point during the mission by a module defined to give it. These can be set to either spread the reward amongst participants or go to a specific player (the closest one, the person who triggered it, the person who found a specific thing, etc). Mission specific items such as documents etc can also be placed in the world for players to pick up and deliver to a location

Co-op:
The jobs system was not originally designed to support co-op very well. A job is accepted by one person and they get the reward usually. They can get friends to help them but those players would not be rewarded most of the time. Missions should be designed from the ground up to support any number of players and/or groups completing them, and cooperation should be encouraged. Mission-level rewards are then distributed evenly.

Resistance Missions:
Officers should be able to create missions that feed directly into the mission list for players to accept and shown alongside the spawned ones. These would be simple, we wont design a full on mission-editor here, just things like "Clear this area of enemies", "Put posters up here", and that kind of thing. These should also be config driven and using the same modules as spawned missions, so the ability for server owners to design complex ones via workbench is still possible. Resistance missions can be assigned a cash reward paid for from the Resistance Funds, those funds are put into escrow at mission creation. Officers can assign these missions to specific groups (1 per group at a time) or just put them on the mission list to be accepted by anyone. This system would only be available to hosted/dedicated server MP games, single players wouldnt make sense assigning missions to themselves and paying themselves.

For missions such as "clear this area" the rewards could be handed out on each kill so skilled players can benefit more. The officer can assign a completion reward still (put into escrow) and then a "per-kill" reward that comes out of the resistance funds on each kill (if theres no funds available it just doesnt pay, but notifies both the participant and the officers that this happened so they can rectify it manually if they want to)

Resistance missions dont reward XP on top of the XP they would get anyway. This would be an exploit vector.

Intel:
This epic will feed into the future "Intel" epic. See github issue #11 for the draft plans for that system. Missions may change intel levels (ie "steal some documents") as well as spawn missions when the resistance is aware of the enemy's movements (ie "ambush this patrol"). These epics will be intertwined while also being independant of each other.

Waypoints:
The current mission waypoint, if one exists (some stages may not be tied to a specific location) is always shown on the map for the participants, they dont need to add it to the map  like they do currently. 

MVP:
For the MVP we provide a basic set of modules + missions and near-parity with what the Job system is currently doing, with a few missions on top that really show what the new system is capable of. Once the systems are tested and stable we can expand on it creating more modules and mission designs plus the stretch goals below

Tooling:
A stretch goal. But if possible we try to create some tooling around missions in the workbench to make them easier to author. Possibly borrowing from the behaviour tree systems as they provide a node-based editor that could be leveraged

Dialog system:
Also a stretch goal, but a very simple dialog system could be created to allow NPCs to relay information and add some more flavour to missions. It doesnt need to be RPG-level and is definitely text-only but it should still allow a question/answer style of gameplay.

Recruits:
Post MVP as well. Missions may assign temporary or even permanent recruits to a player. For example "rescue" missions where you go somewhere to pick someone up, they join your group as a recruit so you can give them orders, put them in vehicles etc. Depending on the mission defined that recruit either leaves your group once you deliver them to another location or stays in your group and becomes a fully fledged member
