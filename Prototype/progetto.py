from random import randint

simulate = True

class Request:
    def __init__(self, _question, _asker):
        self.question = _question
        self.asker = _asker
        self.data = []
        self.answers = 0

class DiscoveryServer:
    def __init__(self, numNodes):
        self.nei_chance = 2
        self.nodes = [] # {(int)port : (Node)node}
        for i in range(numNodes):
            self.addNode()

    def addNode(self, nodePort=-1):
        if nodePort == -1:
            nodePort = randint(0, 9999)
        new_node = Node(nodePort)
    #   done = len(self.nodes) < 1
    #   while not done:
    #       for p, n in self.nodes.items():
    #           if randint(0, self.nei_chance - 1) == 0:
    #               done = True
    #               n.addNeighbours({nodePort : new_node})
    #               new_node.addNeighbours({p : n})
    #   self.nodes[nodePort] = new_node

        nid = len(self.nodes)
        if nid == 0:
            #print(nid)
            #print("\n")
            self.nodes.append(new_node)
        else:
            i = 1
            #print(f"{nid} matches with:")
            while nid % i == 0:
                #print(nid-i)
                new_node.addNeighbours({nid - i : self.nodes[nid - i]})
                self.nodes[nid - i].addNeighbours({nid: new_node})
                i *= 2
            self.nodes.append(new_node)
            #print("\n")


    def getNode(self, nodePort):
        return self.nodes[nodePort]

    def log(self):
        print(f"ho {len(self.nodes)} elementi:")
        for _, n in enumerate(self.nodes):
            n.log()

class Node:
    def __init__(self, _port):
        self.port = _port
        self.neighbours = {} # {(int)port : (Node)node}
        self.active_requests = {} # {(int)id : (Request):req}
        self.pending_requests = {} # {(int)id : (Request):req}

    def addNeighbours(self, nei_list):
        self.neighbours.update(nei_list)

    def ask(self, req_id, req):
        self.active_requests.update({req_id : req})

    def answer(self, req_id, ans):
        self.pending_requests[req_id].answers += 1
        self.pending_requests[req_id].answer.append(ans)
        print(f"{self.port}.answer({req_id}, {ans}) -> {self.pending_requests[req_id].answers}")
        

    def step(self):
        for arid, ar in self.active_requests.items():
            if arid in self.pending_requests:
                asker = self.neighbours[ar.asker]
                asker.answer(arid, [])
            else:
                for p, n in self.neighbours.items():
                    n.ask(arid, Request(ar.question, self.port))
                self.pending_requests.update({arid : ar})
            #del self.active_requests[arid]
        self.active_requests = {}

        to_remove = []
        for prid, pr in self.pending_requests.items():
            if pr.answers == len(self.neighbours) - (0 if pr.asker == -1 else 1):
                pr.answer.append(self.port)
                if pr.asker == -1:
                    print(f"ANSWER: {pr.answer}")
                    simulate = False
                else:
                    asker = self.neighbours[pr.asker]
                    asker.answer(prid, pr.answer)
                #del self.pending_requests[prid]
                to_remove.append(prid)
        self.pending_requests = {k:v for (k,v) in self.pending_requests.items() if k not in to_remove}


    def log(self):
        print(f"ciao sono {self.port}\ti miei vicini sono {[p for p in self.neighbours.keys()]}\n\tpending requests: {self.pending_requests}\n\tactive_requests: {self.active_requests}")

def main():
    ds = DiscoveryServer(129)
    ds.log()
#   ds.addNode(1337)
#   ds.log()
#   print("\n##########\n")
#
#   ds.getNode(1337).ask(42, Request("my question", -1))
#
#   while simulate:
#       for n in ds.nodes.values():
#           n.step()
#           n.log()

if __name__ == "__main__":
    main()
