import numpy as np
from delaunay2D import Delaunay2D
from matplotlib import pyplot as plt
from random import randint
import matplotlib.tri
from time import sleep

class Node:
    def __init__(self, port, grid):
        self.port = port
        self.grid = grid
        self.neighbours = []
        self.active_requests = []
        # [{'id' : id, 'parent' : parent, 'question' : question}]
        # [{'id' : int,'parent' : port  , 'question' : string  }]
        self.pending_requests = {}
        # {id : {'parent' : parent, 'question' : question, 'data' : {neighbour1 : {'answered' : answered, 'answer' : [answer]}, neighbour2 : {}, ...}}}
        # {int: {'parent' : port  , 'question' : string  , 'data' : {port       : {'answered' : bool    , 'answer' : [string]}, port       : {}, ...}}}
        self.pending_neighbours_update = []
        self.secret = f'my port: {port}'

    def updateNeighbours(self, updated_neighbours):
        self.pending_neighbours_update = updated_neighbours
        
    def ask(self, req_id, other, question):
        #print(f"{other} asked me ({self.port}) for question number {req_id}")
# add req to active_requests
        self.active_requests.append({'id' : req_id, 'parent' : other, 'question' : question})
        #print(f"now my active requests list is {self.active_requests}")

    def answer(self, req_id, other, data):
        #print(f"I ({self.port}) got answered about question ({req_id}) by {other}")
# set neighbour as 'has responded' for this request and add response
        self.pending_requests[req_id]['data'][other]['answered'] = True
        self.pending_requests[req_id]['data'][other]['answer'] = data

    def step(self):
        """
        if none pending_requests
            neighbours = pending_neighbours_update
            pending_neighbours_update = []
        for each active request
            if in pending request there's a cycle
                answer with empty
            else
                create new pending requests
                for each neighbour not parent of request
                    set neighbour as 'not yet responded' in pending request
                    ask neighbour this request from self
            remove active request

        for each pending_request
            if all neighbours have responded
                answer parent of request with neighbour's and self's data
                remove pending_request
        """
        if len(self.pending_requests) == 0 and len(self.pending_neighbours_update) != 0:
            self.neighbours = self.pending_neighbours_update
            self.pending_neighbours_update = []
        to_be_deleted = []
        for ar in self.active_requests:
            ar_id = ar['id']
            #print(f"ar_id {ar_id}\ttbd {to_be_deleted}")
            #if ar_id not in to_be_deleted:
            if ar_id in self.pending_requests.keys() or ar_id in to_be_deleted:
                #print(f"req {ar_id} is a cycle")
                self.grid.getNode(ar['parent']).answer(ar_id, self.port, [])
            else:
                this_data = {}
                for n in self.neighbours:
                    if n != ar['parent']:
                        this_data.update({n : {'answered' : False, 'answer' : []}})
                        self.grid.getNode(n).ask(ar_id, self.port, ar['question'])
                self.pending_requests.update({ar_id : {'parent' : ar['parent'], 'question' : ar['question'], 'data' : this_data}})
            to_be_deleted.append(ar_id)
        #for tbd in to_be_deleted:
            #del self.active_requests[tbd]
        #print(f"=======BEGIN==========\nHi! I'm {self.port}. Neighbours\n{self.neighbours}\nActive requests\n{self.active_requests}\nPending requests\n{self.pending_requests}")
        self.active_requests = []
        to_be_deleted = []
        for pr_id, pr in self.pending_requests.items():
            #print(f"checking pr_id {pr_id} -> {pr}")
            if all([nei['answered'] for nei in pr['data'].values()]):
                #print("All neighbours have answered!")
                # that would be a good time to check the question...
                if pr['question'] == 'sum':
                    current_sum = int(self.secret.split(': ')[1])
                    for nei in pr['data'].values():
                        if nei['answer'] != []:
                            current_sum += nei['answer'][0] 
                    data = [current_sum]
                elif pr['question'] == 'avg':
                    current_sum = float(self.secret.split(': ')[1])
                    current_count = 1
                    for nei in pr['data'].values():
                        if nei['answer'] != []:
                            current_sum += float(nei['answer'][1])
                            current_count += int(nei['answer'][2])
                    data = [round((current_sum)/current_count, 2), current_sum, current_count]
                    #print(f"{self.secret} says data is ", data)

                    #current_sum = sum([float(nei['answer'][0]) for nei in pr['data'] if nei['answer'] != []])
                    #current_count = sum([int(nei['answer'][1]) for nei in pr['data'] if nei['answer'] != []]) + 1
                    #data = [round((current_sum + int(self.secret.split(': ')[1]))/ current_count, 2), current_count]
                elif pr['question'] == 'min':
                    min_val = 9999
                    for nei in pr['data'].values():
                        if nei['answer'] != []:
                            min_val = min(min_val, nei['answer'][0])
                    data = [min(int(self.secret.split(': ')[1]), min_val)]
                elif pr['question'] == 'max':
                    max_val = 0
                    for nei in pr['data'].values():
                        if nei['answer'] != []:
                            max_val = max(max_val, nei['answer'][0])
                    data = [max(int(self.secret.split(': ')[1]), max_val)]
                else:
                    data = [self.secret]
                    for nei in pr['data'].values():
                        data.extend(nei['answer'])

                if pr['parent'] < 0:
                    #print("*****************I've asked and got this answer:***************\n", data)
                    #print(f"sending answer to pr_id {pr_id}")
                    self.grid.the_answer[pr_id] = data
                else:
                    self.grid.getNode(pr['parent']).answer(pr_id, self.port, data)
                to_be_deleted.append(pr_id)
        for tbd in to_be_deleted:
            del self.pending_requests[tbd]
        #print(f"=======END============")

class Grid:
    def __init__(self):
        self.nodes = {}
        # {index : {'port' : port, 'node' : node, 'neighbours' : [neighbours]}}
        # {int   : {'port' : int , 'node' : Node, 'neighbours' : [port      ]}}
        self.coords = np.empty((0, 2), int)
        self.dt = Delaunay2D()
        self.the_answer = []
        self.req_id = 0

    def addNode(self, port):
        if port > 9999:
            print("port must be lower than 10000")
            exit(1)
        elif port < 100:
            x, y = 0, port
        else:
            x, y = int(str(port)[:-2]), int(str(port)[-2:])
        
        if port in [n['port'] for n in self.nodes.values()]:
            print(f"{port} already exists in grid")
            return
        p = [x, y]
        self.nodes.update({len(self.nodes.keys()) : {'port' : port, 'node' : Node(port, self), 'neighbours' : []}})
        self.coords = np.append(self.coords, [p], axis = 0)

        self.dt.addPoint(p)

        dt_tris = self.dt.exportTriangles()
        #print("dt_tris", dt_tris)
        for index, node in self.nodes.items():
            # self.nodes[index]['neighbours'] = []
            neighbours = []
            for tris in dt_tris:
                if index in tris:
                    #self.nodes[index]['neighbours'].extend([t for t in tris if t not in self.nodes[index]['neighbours'] and t != p])
                    neighbours.extend([t for t in tris if t not in neighbours and t != index])
            if neighbours != self.nodes[index]['neighbours']:
                self.nodes[index]['node'].updateNeighbours([ self.nodes[n]['port'] for n in neighbours])
                self.nodes[index]['neighbours'] = neighbours
        print(f"node {port} added")
    
    def removeNode(self, port):
        ports = [n['port'] for n in self.nodes.values() if n['port'] != port]
        self.nodes = {}
        self.coords = np.empty((0, 2), int)
        self.dt = Delaunay2D()
        for p in ports:
            self.addNode(p)
        print(f"node {port} removed")

    def ask(self, port, question):
        self.the_answer.append([])
        if port not in [n['port'] for n in self.nodes.values()]:
            print(f"cant find node with port {port}\navailable nodes are:\n{self.nodes}")
            return
        self.getNode(port).ask(self.req_id, -1, question)
        self.req_id += 1

    def simulate(self):
        steps = 0
        for ansi, _ in enumerate(self.the_answer):
            while self.the_answer[ansi] == []:
                for n in self.nodes.values():
                    n['node'].step()
                steps += 1
            print(f"%%%%%%%%ANSWER {ansi} took {steps} steps%%%%%%%%%\n", self.the_answer[ansi], f"\n{len(self.the_answer[ansi])} values total")
            #ansi = []

    def getNode(self, port):
        #print("getnode ", port)
        for n in self.nodes.values():
            if n['port'] == port:
                return n['node']

    def showGrid(self):
        fig, ax = plt.subplots()
        cx, cy = zip(*self.coords)
        dt_tris = self.dt.exportTriangles()
        ax.triplot(matplotlib.tri.Triangulation(cx, cy, dt_tris), 'bo-')
        plt.show()
        input()


def main():
    i = int(input("How many nodes? "))
    g = Grid()
    while len(g.nodes) < i:
        val = randint(100, 9999)
        g.addNode(val)
    done = False
    while not done:
        choice = input(f"\n=============\ngrid size: {len(g.nodes)}\n\n[1] new grid\n[2] add nodes\n[3] remove node\n[4] ask grid\n[5] simulate grid\n[6] show grid\n\n[0] quit\n\n> ")
        if choice == '1':
            g = Grid()
            for i in range(int(input("How many nodes? "))):
                val = randint(100, 9999)
                g.addNode(val)
            pass
        if choice == '2':
            i = int(input("How many nodes? ")) + len(g.nodes)
            while len(g.nodes) < i:
                val = randint(100, 9999)
                g.addNode(val)
            pass
        if choice == '3':
            port = int(input("Which node (port) "))
            g.removeNode(port)
            pass
        if choice == '4':
            port = int(input("Which node (port) "))
            question = input("question: ")
            g.ask(port, question)
            pass
        if choice == '5':
            g.simulate()
            pass
        if choice == '6':
            g.showGrid()
            pass
        if choice == '0':
            done = True

    print("Goodbye!")

if __name__ == '__main__':
    main()
