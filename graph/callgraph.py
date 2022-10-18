#!/usr/bin/python

import sys
import os
import re

# this is just a quick hack to evaluate possibilities of analyzing a callgraph

class Callgraph:

    adjlist = {}
    id2name = {}
    name2id = {}

    reachableFrom = {}

    def __init__(self):
        pass

    def add_node(self, name, id):
        if(id not in self.id2name):
            self.id2name[id] = name
            self.name2id[name] = id
            self.adjlist[id] = []

    def add_edge(self, fromId, toId):
        self.adjlist[fromId].append(toId)
    
    def print(self):
        print(self.id2name)
        print(self.adjlist)
    
    def direct_calls(self):
        calls = {}
        for pair in self.adjlist.items():
            fromId = self.id2name[pair[0]]
            called = list(map(lambda id: self.id2name[id], pair[1]))
            calls[fromId] = called
        return calls

    def transitive_calls(self):
        self.transitive_hull()
        calls = {}
        for pair in self.reachableFrom.items():
            fromId = self.id2name[pair[0]]
            called = list(map(lambda id: self.id2name[id], pair[1]))
            called.sort()
            calls[fromId] = called
        return calls

    # TODO: can be done more efficiently but we need to remap the ids
    # and use a sparse matrix to do this
    # note that the matrix will in general have many nodes (= functions called)
    # but is usually sparse and we need to exploit this
    # DFS as here is the way to go but the data structures can be improved
    def __compute_reachable_from(self, id):
        if id not in self.reachableFrom:
            self.reachableFrom[id] = set() # avoid duplicates
            adjacent = self.adjlist[id]
            for toId in adjacent:
                self.__compute_reachable_from(toId)
                self.reachableFrom[id] = self.reachableFrom[id].union(self.reachableFrom[toId])
                self.reachableFrom[id].add(toId)

    def transitive_hull(self) :
        self.reachableFrom = {}
        for id in self.adjlist:
            self.__compute_reachable_from(id)

def generate_graph(filename):
    command = "clang++ -S -emit-llvm " + filename + " -o - | opt --dot-callgraph - "
    os.system(command)

def demangle(outputName):
    command = "llvm-cxxfilt < \"<stdin>.callgraph.dot\" > " + outputName
    print(command)
    os.system(command)

def visualize_callgraph():
    command = "dot -Tpng -ocallgraph.png \"<stdin>.callgraph.dot\""
    os.system(command)

def parse_graph(callgraph):
    nodePattern = r'Node0x([0-9a-f]+).*label=\"{(.*)}'
    edgePattern = r'Node0x([0-9a-f]+) -> Node0x([0-9a-f]+)'

    file = open(callgraph, 'r')
    lines = file.readlines()

    graph = Callgraph()

    for line in lines:
        m = re.findall(nodePattern, line)
        if len(m) > 0:
            id = int(m[0][0], 16)
            graph.add_node(m[0][1], id)
        else:
            m = re.findall(edgePattern, line)
            if len(m) > 0:
                fromId = int(m[0][0], 16)
                toId = int(m[0][1], 16)
                graph.add_edge(fromId, toId)
    return graph

# TODO: can be used to filter for calls with a prefix (e.g. namespace),
# but later a more sophisticated way (via clang AST) to filter
# functions defined by the library should be used (how does this work
# with expanded templates from say STL though?)
def filter_prefix(calls, prefix):
    pattern = r'^' + re.escape(prefix)
    filtered = []
    for pair in calls.items():
        m = re.findall(pattern, pair[0])
        if len(m) > 0:
            filtered.append((pair[0], pair[1]))

    filtered.sort()    
    return filtered

def write_output_file(calls, filename):
    f = open(filename, "w")
    f.seek(0)
    i = 1
    for pair in calls:
        line = str(i) + ": " + pair[0] + " calls: " + str(pair[1]) + "\n\n"
        f.write(line)
        i += 1
    f.close()

def main():
    filename = sys.argv[1]
    # TODO use options, parse whole compilation unit
    print("processing ", filename)

    generate_graph(filename)
    visualize_callgraph()
    demangle("demangled_graph.dot")

    # we parse and process the graph information
    graph = parse_graph("demangled_graph.dot")

    transitiveCalls = graph.transitive_calls()
    for pair in transitiveCalls.items():
         print(pair[0], " calls: ", pair[1], "\n")

    # TODO: filter as script args etc.
    filteredCalls = filter_prefix(transitiveCalls, "hap::")
    #filteredCalls = filter_prefix(transitiveCalls, "")

    s = filename.split('.')
    outfileName = s[0] + "_calls.txt"
    write_output_file(filteredCalls, outfileName)

if __name__ == "__main__":
    main()