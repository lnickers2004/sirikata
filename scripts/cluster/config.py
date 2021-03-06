#!/usr/bin/python

import os.path

class ClusterNode:
    def __init__(self, spec_string):
        (self.user, rest) = spec_string.split('@',1)
        rest = rest.split(':', 1)
        self.node = rest[0]
        if (len(rest) > 1):
            self.reuse_count = int(rest[1])
        else:
            self.reuse_count = 1

    def str(self):
        return self.user + "@" + self.node

    def __str__(self):
        return self.user + "@" + self.node + ":" + str(self.reuse_count)

    def __repr__(self):
        return "<" + self.user + "@" + self.node + ":" + str(self.reuse_count) + ">"


def remove_comment(line):
    # FIXME this is rather simplistic, assumes # never appears, e.g., quoted
    before, commenter, after = line.partition('#')
    # No comment character
    if (len(before) == len(line)):
        return line
    return before

def parse_bool(opt):
    simple_opt = opt.strip().lower()
    if (simple_opt in ['true', 't', 'on', 'y', 'yes']):
        return True
    return False

class ClusterConfig:
    def __init__(self):
        #default values
        self.headnode="bogus@bogus"
        self.timeserver = None
        self.nodes = []
        self.deploy_nodes = []
        self.repository = 'git://github.com/sirikata/sirikata.git'
        self.branch = "master"
        self.code_dir = "cbr"
        self.pack_dir = ""
        self.oseg_code_dir = "oseg.git"
        self.port_base = 6666
        self.unique = None
        self.ccache = False

        self.plugins = 'tcpsst,servermap-tabular,core-local'
        self.space_plugins = 'weight-exp,weight-sqr,weight-const,space-craq,space-local,space-standard,space-master-pinto'
        self.cseg_plugins = 'weight-exp,weight-sqr,weight-const'
        self.pinto_plugins = ''
        self.simoh_plugins = 'weight-exp,weight-sqr,weight-const'
        self.analysis_plugins = 'weight-exp,weight-sqr,weight-const'

        self.zookeeper = ""
        self.zookeeper_addr = ""
        self.craq_nodes = ""

        # parse default config or generate a default config
        default_path = os.path.expanduser("~/.cluster")
        if (os.path.exists(default_path)):
            self.parse(default_path)
        else: # just setup some default nodes that make sense for us
            print "Couldn't find ~/.cluster configure file."
            assert(False)

    def generate_deployment(self, count, repeat = True):
        if (self.nodes == None or len(self.nodes) == 0):
            return None

        max_copies = max( [x.reuse_count for x in self.nodes] )
        nodes_by_count = [ [] ]
        for reuse in range(1,max_copies+1):
            nodes_by_count.append( [] )
        for node in self.nodes:
            nodes_by_count[node.reuse_count].append(node)

        self.deploy_nodes = []
        while( True ):
            for used_count in range(1,max_copies+1):
                for cur_count in range(used_count,max_copies+1):
                    for node in nodes_by_count[cur_count]:
                        self.deploy_nodes.append(node)
                        if len(self.deploy_nodes) == count:
                            return self.deploy_nodes
        return None

    # Parse options from the given file
    def parse(self, filename):
        fp = open(filename, 'r')
        if (fp == None):
            return
        # clear out any old values
        self.user = None
        self.nodes = []
        # line format is option = value
        for line in fp:
            line = remove_comment(line)
            line_split = line.split('=', 1)
            if (len(line_split) < 2):
                continue
            [opt_name, opt_value] = line_split
            opt_name = opt_name.strip()
            if (opt_name == "headnode"):
                self.headnode = opt_value.strip()
            elif (opt_name == "timeserver"):
                self.timeserver = opt_value.strip()
            elif (opt_name == "pack_dir"):
                self.pack_dir = opt_value.strip()
            elif (opt_name == "node"):
                self.nodes.append( ClusterNode(opt_value.strip()) )
            elif (opt_name == "repository"):
                self.repository = opt_value.strip()
            elif (opt_name == "branch"):
                self.branch = opt_value.strip()
            elif (opt_name == "code_dir"):
                self.code_dir = opt_value.strip()
            elif (opt_name == "oseg_code_dir"):
                self.oseg_code_dir = opt_value.strip()
            elif (opt_name == "port"):
                self.port_base = int(opt_value)
            elif (opt_name == "unique"):
                opt_value = opt_value.strip()
                self.unique = opt_value[0]
                if (len(opt_value) > 1):
                    print 'Warning: unique string instead of character, using first char: "' + opt_value + '=> "' + self.unique
            elif (opt_name == 'ccache'):
                self.ccache = parse_bool(opt_value)
            elif (opt_name == 'zookeeper'):
                self.zookeeper = opt_value.strip()
            elif (opt_name == 'zookeeper_addr'):
                self.zookeeper_addr = opt_value.strip()
            elif (opt_name == 'craq_nodes'):
                self.craq_nodes = eval(opt_value)

            elif (opt_name == 'plugins'):
                self.plugins = opt_value.strip()
            elif (opt_name == 'space_plugins'):
                self.space_plugins = opt_value.strip()
            elif (opt_name == 'cseg_plugins'):
                self.cseg_plugins = opt_value.strip()
            elif (opt_name == 'simoh_plugins'):
                self.simoh_plugins = opt_value.strip()
            elif (opt_name == 'analysis_plugins'):
                self.analysis_plugins = opt_value.strip()

        fp.close()
        return


if __name__ == "__main__":
    testcc = ClusterConfig()
    print "Nodes: ", testcc.nodes
    print "Code Directory: ", testcc.code_dir
    print "2 Deployment: ", testcc.generate_deployment(2)
    print "6 Deployment: ", testcc.generate_deployment(6)
    print "6 Deployment, No Repeat: ", testcc.generate_deployment(6, False)
    print "9 Deployment: ", testcc.generate_deployment(9)
