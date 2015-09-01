/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of Natron <http://www.natron.fr/>,
 * Copyright (C) 2015 INRIA and Alexandre Gauthier-Foichat
 *
 * Natron is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Natron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Natron.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

// ***** BEGIN PYTHON BLOCK *****
// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>
// ***** END PYTHON BLOCK *****

#include "NodeGraphPrivate.h"
#include "NodeGraph.h"

#include "Engine/Node.h"
#include "Engine/NodeGroup.h"
#include "Engine/NodeSerialization.h"
#include "Engine/Project.h"

#include "Gui/Edge.h"
#include "Gui/Gui.h"
#include "Gui/GuiAppInstance.h"
#include "Gui/GuiApplicationManager.h" // appPTR
#include "Gui/NodeClipBoard.h"
#include "Gui/NodeGui.h"
#include "Gui/NodeGuiSerialization.h"


using namespace Natron;


NodeGraphPrivate::NodeGraphPrivate(Gui* gui,
                                   NodeGraph* p,
                                   const boost::shared_ptr<NodeCollection>& group)
: _publicInterface(p)
, _gui(gui)
, group(group)
, _lastMousePos()
, _lastNodeDragStartPoint()
, _lastSelectionStartPoint()
, _evtState(eEventStateNone)
, _magnifiedNode()
, _nodeSelectedScaleBeforeMagnif(1.)
, _magnifOn(false)
, _arrowSelected(NULL)
, _nodesMutex()
, _nodes()
, _nodesTrash()
, _nodeCreationShortcutEnabled(false)
, _lastNodeCreatedName()
, _root(NULL)
, _nodeRoot(NULL)
, _cacheSizeText(NULL)
, _refreshCacheTextTimer()
, _navigator(NULL)
, _undoStack(NULL)
, _menu(NULL)
, _tL(NULL)
, _tR(NULL)
, _bR(NULL)
, _bL(NULL)
, _refreshOverlays(false)
, _highLightedEdge(NULL)
, _mergeHintNode()
, _hintInputEdge(NULL)
, _hintOutputEdge(NULL)
, _backdropResized()
, _selection()
, _nodesWithinBDAtPenDown()
, _selectionRect(NULL)
, _bendPointsVisible(false)
, _knobLinksVisible(true)
, _accumDelta(0)
, _detailsVisible(false)
, _deltaSinceMousePress(0,0)
, _hasMovedOnce(false)
, lastSelectedViewer(0)
, wasLaskUserSeekDuringPlayback(false)
{
}

void
NodeGraphPrivate::resetSelection()
{
    for (std::list<boost::shared_ptr<NodeGui> >::iterator it = _selection.begin(); it != _selection.end(); ++it) {
        (*it)->setUserSelected(false);
    }

    _selection.clear();
}

void
NodeGraphPrivate::editSelectionFromSelectionRectangle(bool addToSelection)
{
    if (!addToSelection) {
        resetSelection();
    }

    QRectF selection = _selectionRect->mapToScene( _selectionRect->rect() ).boundingRect();

    for (NodeGuiList::iterator it = _nodes.begin(); it != _nodes.end(); ++it) {
        QRectF bbox = (*it)->mapToScene( (*it)->boundingRect() ).boundingRect();
        if ( selection.contains(bbox) ) {
            
            NodeGuiList::iterator foundInSel = std::find(_selection.begin(),_selection.end(),*it);
            if (foundInSel != _selection.end()) {
                continue;
            }
            
            _selection.push_back(*it);
            (*it)->setUserSelected(true);
        }
    }
}

void
NodeGraphPrivate::rearrangeSelectedNodes()
{
    if ( !_selection.empty() ) {
        _publicInterface->pushUndoCommand( new RearrangeNodesCommand(_selection) );
    }
}

void
NodeGraphPrivate::setNodesBendPointsVisible(bool visible)
{
    _bendPointsVisible = visible;

    for (std::list<boost::shared_ptr<NodeGui> >::iterator it = _nodes.begin(); it != _nodes.end(); ++it) {
        const std::vector<Edge*> & edges = (*it)->getInputsArrows();
        for (std::vector<Edge*>::const_iterator it2 = edges.begin(); it2 != edges.end(); ++it2) {
            if (visible) {
                if ( !(*it2)->isOutputEdge() && (*it2)->hasSource() && ((*it2)->line().length() > 50) ) {
                    (*it2)->setBendPointVisible(visible);
                }
            } else {
                if ( (*it2) && !(*it2)->isOutputEdge() ) {
                    (*it2)->setBendPointVisible(visible);
                }
            }
        }
    }
}


QRectF
NodeGraphPrivate::calcNodesBoundingRect()
{
    QRectF ret;
    QMutexLocker l(&_nodesMutex);

    for (std::list<boost::shared_ptr<NodeGui> >::iterator it = _nodes.begin(); it != _nodes.end(); ++it) {
        if ( (*it)->isVisible() ) {
            ret = ret.united( (*it)->boundingRectWithEdges() );
        }
    }
 
    return ret;
}


void
NodeGraphPrivate::resetAllClipboards()
{
    appPTR->clearNodeClipBoard();
}


void
NodeGraphPrivate::copyNodesInternal(const NodeGuiList& selection,NodeClipBoard & clipboard)
{
    ///Clear clipboard
    clipboard.nodes.clear();
    clipboard.nodesUI.clear();

    NodeGuiList nodesToCopy = selection;
    for (NodeGuiList::iterator it = nodesToCopy.begin(); it != nodesToCopy.end(); ++it) {
        ///Also copy all nodes within the backdrop
        std::list<boost::shared_ptr<NodeGui> > nodesWithinBD = _publicInterface->getNodesWithinBackDrop(*it);
        for (std::list<boost::shared_ptr<NodeGui> >::iterator it2 = nodesWithinBD.begin(); it2 != nodesWithinBD.end(); ++it2) {
            std::list<boost::shared_ptr<NodeGui> >::iterator found = std::find(nodesToCopy.begin(),nodesToCopy.end(),*it2);
            if ( found == nodesToCopy.end() ) {
                nodesToCopy.push_back(*it2);
            }
        }
    }
    
    for (NodeGuiList::iterator it = nodesToCopy.begin(); it != nodesToCopy.end(); ++it) {
        if ((*it)->isVisible()) {
            boost::shared_ptr<NodeSerialization> ns( new NodeSerialization( (*it)->getNode(), true ) );
            boost::shared_ptr<NodeGuiSerialization> nGuiS(new NodeGuiSerialization);
            (*it)->serialize( nGuiS.get() );
            clipboard.nodes.push_back(ns);
            clipboard.nodesUI.push_back(nGuiS);
        }
    }
}