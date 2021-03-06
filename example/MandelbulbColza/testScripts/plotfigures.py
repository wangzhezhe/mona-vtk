import matplotlib.pyplot as plt
from itertools import cycle
import numpy as np
import json
from matplotlib.ticker import PercentFormatter
from matplotlib.patches import Patch
import matplotlib

# google color
gblue = '#4486F4'
glered = '#DA483B'
gyellow = '#FFC718'
ggreen = '#1CA45C'
slightred = '#F00285'
deepblue= '#58508d'
purple='#AB6EC4'
orange='#F7710A'

def strong_scale():
    fig, ax = plt.subplots(figsize=(8,4.6))
    ax.set_xlabel('The number of the data staging processes', fontsize='large')
    ax.set_ylabel('Time(s)', fontsize='large')  

    # set tick
    N = 6
    ind = np.array([1,2,4,8,16,32])/32.0   # the x locations 
    width = 0.2       # the width of the bars
    ax.set_xticks(ind*width)
    ax.set_xticklabels(('4','8','16','32','64','128'), fontsize='large')
    ax.set_ylim([0,45])

    avg_mona_8M=(40.775775,	20.5655,	10.64108,	5.555285,	3.07131,	1.879173333)
    avg_mpi_8M=(40.9562,	20.9955,	10.90106667,	5.659426667,	3.067103333,	1.873846667)
    p11=ax.plot(ind*width,avg_mona_8M, color=gblue, linestyle='-', marker='o', label="Mona 8MB")
    p12=ax.plot(ind*width,avg_mpi_8M, color=glered, linestyle='--',marker='.',label="MPI 8MB")

    avg_mona_4M=(22.197025,	11.43235,	5.9381975,	3.243511667,	1.890893333,	1.25604)
    avg_mpi_4M=(22.4269,	11.51185,	5.9681225,	3.26737,	1.952343333,	1.238748)
    p21=ax.plot(ind*width,avg_mona_4M, color=gyellow, linestyle='-', marker='o',label="Mona 4MB")
    p22=ax.plot(ind*width,avg_mpi_4M, color=ggreen, linestyle='--',marker='.',label="MPI 4MB")

    avg_mona_2M=(14.3906,	7.614742,	4.159266,	2.403358,	1.54398,	1.184746)
    avg_mpi_2M=(14.56363333,	7.550728,	4.155902,	2.455214,	1.563652,	1.174748333)
    p21=ax.plot(ind*width,avg_mona_2M, color=purple, linestyle='-', marker='o',label="Mona 2MB")
    p22=ax.plot(ind*width,avg_mpi_2M, color=orange, linestyle='--',marker='.',label="MPI 2MB")

    #legend = ax.legend(handles=legend_elem, loc='upper center', ncol=2, bbox_to_anchor=(0.6, 1.0), fontsize=12)
    legend = ax.legend(loc='upper center', ncol=3, bbox_to_anchor=(0.6, 1.0), fontsize=12)

    plt.savefig("strong_scale.pdf",bbox_inches='tight')
    plt.savefig("strong_scale.png",bbox_inches='tight')


def weak_scale():
    fig, ax = plt.subplots(figsize=(8,4.6))
    ax.set_xlabel('Client process number : Staging process number', fontsize='large')
    ax.set_ylabel('Time(s)', fontsize='large')  

    # set tick
    N = 5
    ind = np.arange(N)  # the x locations 
    width = 0.2       # the width of the bars
    ax.set_xticks(ind*width)
    ax.set_xticklabels(('64:8','128:16','256:32','512:64','1024:128'), fontsize='large')
    ax.set_ylim([0,5])

    avg_mona_8M=(3.8479925,	3.73247,	3.785483333,	3.62453,	3.698273333)
    avg_mpi_8M=(3.942175,	3.73247,	3.778903333,	3.64829,	3.651495)
    p11=ax.plot(ind*width,avg_mona_8M, color=gblue, linestyle='-', marker='o', label="Mona 8MB")
    p12=ax.plot(ind*width,avg_mpi_8M, color=glered, linestyle='--',marker='.',label="MPI 8MB")

    avg_mona_4M=(2.285118,	2.2872,	2.20086,	2.2685775,	2.2986025)
    avg_mpi_4M=(2.290688,	2.185118,	2.2011,	2.1985,	2.1855)
    p21=ax.plot(ind*width,avg_mona_4M, color=gyellow, linestyle='-', marker='o',label="Mona 4MB")
    p22=ax.plot(ind*width,avg_mpi_4M, color=ggreen, linestyle='--',marker='.',label="MPI 4MB")

    avg_mona_2M=(1.91456,	1.912072,	1.83128,	1.838235,	1.972172)
    avg_mpi_2M=(1.999946,	1.929902,	1.918624,	1.879425,	1.902348)
    p21=ax.plot(ind*width,avg_mona_2M, color=purple, linestyle='-', marker='o',label="Mona 2MB")
    p22=ax.plot(ind*width,avg_mpi_2M, color=orange, linestyle='--',marker='.',label="MPI 2MB")

    #legend = ax.legend(handles=legend_elem, loc='upper center', ncol=2, bbox_to_anchor=(0.6, 1.0), fontsize=12)
    legend = ax.legend(loc='upper center', ncol=3, bbox_to_anchor=(0.6, 1.0), fontsize=12)

    plt.savefig("weak_scale.pdf",bbox_inches='tight')
    plt.savefig("weak_scale.png",bbox_inches='tight')

if __name__ == '__main__':
    strong_scale()
    weak_scale()

'''
There are 4 data blocks for every client, for strong scale case, there are 512 clients and 2048 data blocks in total
for the weak scale case, the number of the client varies with the number of the client
'''