//  Powiter
//
//  Created by Alexandre Gauthier-Foichat on 06/12
//  Copyright (c) 2013 Alexandre Gauthier-Foichat. All rights reserved.
//  contact: immarespond at gmail dot com

#ifndef __PowiterOsX__interest__
#define __PowiterOsX__interest__

#include <iostream>
#include "Core/channels.h"
#include <map>
#include <vector>
#include <QtCore/QFutureWatcher>
#include <QtCore/QFuture>
#include "Core/row.h"
class Node;
/*This class is useful for spatial operators that need several rows, up to the whole image, of an input
 nodes. This object is kind of a generalisation of the node->get() function.
 Note that this object launches new threads to actually get the rows.*/
class Interest : public QObject{
    
    Q_OBJECT
    
    int _x,_y,_r,_t;
    ChannelSet _channels;
    Node* _node;
    bool _isFinished;
    std::map<int,InputRow*> _interest;
    std::vector<InputRow*> _sequence;
    QFutureWatcher<void>* _results;
public:
    /*Construct an interest object that will fetch all the row in the range (y,t)
     for the given channels for the node specified in parameters. No fetching
     actually occurs in the constructor, you have to call claimInterest()
     to start computations. This lazy computation fashion allows you to 
     connect to the signal hasFinishedAt(int) to maybe update your piece
     of code for a specific row, or to signal the GUI of some modifications.
     
     !!WARNING: When the Interest object is deleted, all the InputRows will be
     deleted too, it's important to maintain the Interest living thoughout the
     entire duration you need to use the InputRows contained in that object.*/
    Interest(Node* node, int x, int y, int r, int t, ChannelMask channels);
    
    
    /*Launches new thread to fetch all the rows necessary from the node to fill the range (y,t).
     It is useful to note that the Interest object ,throughout its lifetime,will lock the rows
     into the cache and prevent them from being deleted(this statement is valid only if the node
     caches data , otherwise they will never be cached).
     
     !!WARNING : Results will not be available right away when returning from
     the constructor. You can connect to the signal hasFinishedCompletly to be sure
     that the rows are avaiable. You can also call isFinished().
     You can also connect to the signal hasFinishedAt(int) that will tell you
     when a specific row has been fetch from the input. This may be used
     to start following computation a bit ahead, but this is not really
     useful since the purpose of the interest object is to retrieve a block
     of rows that are inter-dependent; if they were independent you could use
     Node::get in the first place.
     */
    void claimInterest();
    
    bool isFinished() const {return _isFinished;}
    
    /*Returns the row  y computed by the node. Throws an exception
     if it couldn't find it.
     !!WARNING: calling at() while isFinished still returns false
     can produce undefined results and garbage data!*/
    const InputRow& at(int y) const;
    
    /*Deletes all the InputRows in the Interest, and tell the cache they are
     no longer protected (i.e : they return to normal priority and can be evicted).*/
    ~Interest();
    public slots:
    void notifyFinishedAt(int);
    void setFinished();
    
signals:
    /*Thrown when a row has been computed by the interest. The parameter is the y
     coordinate of the row.*/
    void hasFinishedAt(int);
    void hasFinishedCompletly();
    
private:
    void getInputRow(Node* node,InputRow* row);
};

#endif /* defined(__PowiterOsX__interest__) */
