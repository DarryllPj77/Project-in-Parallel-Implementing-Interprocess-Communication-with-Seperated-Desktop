🧾 IPC Communication Between Two VirtualBox Ubuntu Desktops
Using Internal Network Setup
(For Socket-Based IPC Projects like Multiplayer Trivia)

🧩 Overview
We configure two Ubuntu VirtualBox VMs to communicate using an Internal Network with manual IP assignment. This is ideal for local IPC testing without internet access.

1️⃣ VirtualBox Network Setup: Internal Network
Do this for both Desktop1 and Desktop2:
Open VirtualBox

Select the VM (e.g., Ubuntu22_Desktop1)

Click Settings → Network

In Adapter 1:

✅ Check Enable Network Adapter

Attached to: Internal Network

Name: intnet (or create a shared name like IPC_NET)

Click OK

🔁 Repeat for Desktop2 using the same network name.

-----------------------------------------------------------------------------------------------------------------------------
🔴 Problem: Missing IP on enp0s3 After Switching to Internal Network
When using an Internal Network, VirtualBox does not provide a DHCP server, so Ubuntu won’t get an IP automatically.
You’ll notice this with:
bash
ip a
🛑 The output will not show an inet address (like 192.168.x.x) under enp0s3.

2️⃣ Manually Assign IP Addresses
🖥️ On Desktop1 (Server):
bash
sudo ip addr flush dev enp0s3
sudo ip addr add 192.168.56.101/24 dev enp0s3
sudo ip link set enp0s3 up
🖥️ On Desktop2 (Client):
bash
sudo ip addr flush dev enp0s3
sudo ip addr add 192.168.56.102/24 dev enp0s3
sudo ip link set enp0s3 up

3️⃣ Verify IP Assignment
On both desktops, run:

bash
ip a
You should see:
Desktop1: inet 192.168.56.101/24 under enp0s3
Desktop2: inet 192.168.56.102/24 under enp0s3

4️⃣ Test the Connection
From Desktop2, try pinging Desktop1:
bash
ping 192.168.56.101
✅ If successful, you’ll see responses like:
64 bytes from 192.168.56.101: icmp_seq=1 ttl=64 time=0.4 ms
If not:
Ensure IPs are correctly assigned
Try sudo ufw disable on both machines to remove firewall blocking

5️⃣ Update the Client Code
In your client.cpp:

cpp
#define SERVER_IP "192.168.56.101"
Then recompile both programs:

bash
g++ server.cpp -o server -pthread
g++ client.cpp -o client -pthread

6️⃣ Run the Server and Client
On Desktop1 (Server):
bash
./server
On Desktop2 (Client):
bash
./client
✅ The client should connect to the server and start communication.

✅ Summary Table
Component	Configuration
NIC Type	Internal Network (intnet)
Desktop1 IP	192.168.56.101
Desktop2 IP	192.168.56.102
Client #define	SERVER_IP "192.168.56.101"
Compile Server	g++ server.cpp -o server
Compile Client	g++ client.cpp -o client
Run	./server & ./client

