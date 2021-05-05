from random import randint

class Request:
    def __init__(self, _question):
        self.question = _question
        self.answer = []

    def __init__(self, req
    
class DiscoveryServer:
    def __init__(self, numNodes):
        self.nei_chance = 2
        self.nodes = {} # {(int)port : (Node)node}
        for i in range(numNodes):
            self.addNode()

    def addNode(self, nodePort=-1):
        if nodePort == -1:
            nodePort = randint(0, 9999)
        new_node = Node(nodePort)
        done = len(self.nodes) < 1
        while not done:
            for p, n in self.nodes.items():
                if randint(0, self.nei_chance - 1) == 0:
                    done = True
                    n.addNeighbours({nodePort : new_node})
                    new_node.addNeighbours({p : n})
        self.nodes[nodePort] = new_node

    def log(self):
        print(f"ho {len(self.nodes)} elementi:")
        for _, n in self.nodes.items():
            n.log()

class Node:
    def __init__(self, _port):
        self.port = _port
        self.neighbours = {} # {(int)port : (Node)node}
        self.active_requests = {} # {(int)id : (Request):req}
        self.pending_requests = {} # {(int)id : (Request):req}

    def addNeighbours(self, nei_list):
        self.neighbours.update(nei_list)

    def log(self):
        print(f"ciao sono {self.port}\ti miei vicini sono {[p for p in self.neighbours.keys()]}")

def main():
    ds = DiscoveryServer(15)
    ds.log()


if __name__ == "__main__":
    main()
