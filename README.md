# Tanks

Brian Goodell and Bradley Ramsey

For this project we made a simple, yet surprisingly fun multiplayer tank game, with a twist: in the background it's a distributed password cracker. 

Essentially, when you join the central hub, it asks for a username and password. It hashes the password so your plaintext version never leaves the local machine, and then add you to the game. It handles player opponent assignments, and then you connect peer-to-peer to play the actual game with reduced latency (you can also change the speed - `HORIZONAL_INTERVAL` and `VERTICAL_INTERVAL` in tank.h - to the point where there's not much noticable lag. In playtest, we just found the game was less chaotic and more strategic at slower speeds). The passwords are conglomerated by the host and sent out to each connected machine. They each crack a section of the search space and then report back on any passwords they found.

To test it out, download the code and run "make." You can test it locally by opening three terminals, one for a host and one for each player. (Note: typing `:<port>` will allow you to connect to a localhost without typing the full hostname). If you test it locally, changing the password length (in cracker-gpu.h) to 6 will allow you to test the password cracking without having to wait too long.

For our at-scale test, we had 12 machines. After connecting, the program assigned partners and they played. Once the game was done and the host user told it to continue, the program assigned new partners based on the previous winners, and started the next round. In the background, it cracked all the users passwords (7 characters, lowercase with numbers) in 55 seconds, although that was likely delayed a bit by waiting for the round to start so it could receive the new messages (an inefficiency that we didn't have time to address). It did 78 billion potential combinations in that time, so almost one and a half billion passwords per second. This was about 50 times faster than the fastest CPU implemention in class - also ours - but our tests indicated it was probably closer to 80 times faster before the round delay).


## Video Demo
*A note: it's clearly been sped up - for the sake of github's size limits and minimizing observer boredom - but the stats printed at the end are also wrong. It was treating the size_t for the searchspace and the float for speed up both as ints. They should be 78364164096 and 11.2 respectively.

https://github.com/bradleyramsey/TanksProject/assets/35513259/aa0a77b8-a818-41ab-91cf-6ed60d179b19

