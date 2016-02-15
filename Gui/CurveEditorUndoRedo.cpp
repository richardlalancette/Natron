/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of Natron <http://www.natron.fr/>,
 * Copyright (C) 2016 INRIA and Alexandre Gauthier-Foichat
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

#include "CurveEditorUndoRedo.h"

#include <cmath>
#include <stdexcept>

#include <QtCore/QDebug>

#include "Global/GlobalDefines.h"

#include "Gui/CurveEditor.h"
#include "Gui/CurveGui.h"
#include "Gui/KnobGui.h"
#include "Gui/CurveWidget.h"

#include "Engine/Bezier.h"
#include "Engine/Knob.h"
#include "Engine/Curve.h"
#include "Engine/RotoContext.h"
#include "Engine/KnobTypes.h"
#include "Engine/Transform.h"
#include "Engine/ViewIdx.h"

NATRON_NAMESPACE_ENTER;

//////////////////////////////ADD MULTIPLE KEYS COMMAND//////////////////////////////////////////////
AddKeysCommand::AddKeysCommand(CurveWidget *editor,
                               const KeysToAddList & keys,
                               QUndoCommand *parent)
    : QUndoCommand(parent)
      , _keys(keys)
      , _curveWidget(editor)
{
}

AddKeysCommand::AddKeysCommand(CurveWidget *editor,
                               const boost::shared_ptr<CurveGui>& curve,
                               const std::vector<KeyFrame> & keys,
                               QUndoCommand *parent)
    : QUndoCommand(parent)
      , _keys()
      , _curveWidget(editor)
{
    _keys.insert(std::make_pair(curve, keys));
}

void
AddKeysCommand::addOrRemoveKeyframe(bool isSetKeyCommand, bool add)
{
    
    
    
    for (std::map<boost::shared_ptr<CurveGui> ,std::vector<KeyFrame> >::iterator it = _keys.begin(); it!=_keys.end(); ++it) {
        
        KnobCurveGui* isKnobCurve = dynamic_cast<KnobCurveGui*>(it->first.get());
        BezierCPCurveGui* isBezierCurve = dynamic_cast<BezierCPCurveGui*>(it->first.get());
        KnobGui* guiKnob = isKnobCurve ? isKnobCurve->getKnobGui() : 0;
        KnobPtr knob;
        
        if (isKnobCurve) {
            knob = isKnobCurve->getInternalKnob();
        }
        boost::shared_ptr<KnobParametric> isParametric;
        if (knob) {
            isParametric = boost::dynamic_pointer_cast<KnobParametric>(knob);
        }

        if (add && isSetKeyCommand) {
            if (isKnobCurve) {
                if (isParametric) {
                    StatusEnum st = isParametric->deleteAllControlPoints(isKnobCurve->getDimension());
                    assert(st == eStatusOK);
                    if (st != eStatusOK) {
                        throw std::logic_error("addOrRemoveKeyframe");
                    }
                } else {
                    knob->removeAnimation(ViewIdx(0),isKnobCurve->getDimension());
                }
            } else if (isBezierCurve) {
                boost::shared_ptr<Bezier> b = isBezierCurve->getBezier();
                assert(b);
                if (b) {
                    b->removeAnimation();
                }
            }
        }
        
        if (guiKnob && !isParametric) {
            if (add && isKnobCurve) {
                guiKnob->setKeyframes(it->second, isKnobCurve->getDimension(), ViewIdx(0) );
            } else {
                guiKnob->removeKeyframes(it->second, isKnobCurve->getDimension(), ViewIdx(0) );
                
            }
        } else {
            
            for (std::size_t i = 0; i < it->second.size(); ++i) {
                if (isKnobCurve) {
                    isKnobCurve->getInternalKnob()->beginChanges();
                    
                    if (add) {
                        int time = it->second[i].getTime();
                        if (isParametric) {
                            
                            StatusEnum st = isParametric->addControlPoint( isKnobCurve->getDimension(), it->second[i].getTime(),it->second[i].getValue() );
                            assert(st == eStatusOK);
                            Q_UNUSED(st);
                        } else {
                            Knob<double>* isDouble = dynamic_cast<Knob<double>*>(knob.get());
                            Knob<bool>* isBool = dynamic_cast<Knob<bool>*>(knob.get());
                            Knob<int>* isInt = dynamic_cast<Knob<int>*>(knob.get());
                            Knob<std::string>* isString = dynamic_cast<Knob<std::string>*>(knob.get());
                            if (isDouble) {
                                isDouble->setValueAtTime(time, ViewIdx(0), isDouble->getValueAtTime(time, 0, ViewIdx(0)), isKnobCurve->getDimension());
                            } else if (isBool) {
                                isBool->setValueAtTime(time, ViewIdx(0), isBool->getValueAtTime(time, 0, ViewIdx(0)), isKnobCurve->getDimension());
                            } else if (isInt) {
                                isInt->setValueAtTime(time, ViewIdx(0),  isInt->getValueAtTime(time, 0, ViewIdx(0)), isKnobCurve->getDimension());
                            } else if (isString) {
                                isString->setValueAtTime(time, ViewIdx(0), isString->getValueAtTime(time, 0, ViewIdx(0)), isKnobCurve->getDimension());
                            }
                        }
                    } else {
                        
                        boost::shared_ptr<KnobParametric> knob = boost::dynamic_pointer_cast<KnobParametric>( isKnobCurve->getInternalKnob() );
                        
                        if (knob) {
                            StatusEnum st = knob->deleteControlPoint( isKnobCurve->getDimension(),
                                                                             it->first->getInternalCurve()->keyFrameIndex( it->second[i].getTime() ) );
                            assert(st == eStatusOK);
                            Q_UNUSED(st);
                        } else {
                            isKnobCurve->getInternalKnob()->deleteValueAtTime(eCurveChangeReasonCurveEditor, it->second[i].getTime(), ViewIdx(0),  isKnobCurve->getDimension() );
                        }
                    }
                } else if (isBezierCurve) {
                    if (add) {
                        isBezierCurve->getBezier()->setKeyframe(it->second[i].getTime());
                    } else {
                        isBezierCurve->getBezier()->removeKeyframe(it->second[i].getTime());
                    }
                } // if (isKnobCurve) {
            } // for (std::size_t i = 0; i < it->second.size(); ++i) {
        } // if (guiKnob) {
        if (isKnobCurve) {
            isKnobCurve->getInternalKnob()->endChanges();
        }
    }
    
    _curveWidget->update();

    setText( QObject::tr("Add multiple keyframes") );
} // addOrRemoveKeyframe

void
AddKeysCommand::undo()
{
    addOrRemoveKeyframe(false, false);
}

void
AddKeysCommand::redo()
{
    addOrRemoveKeyframe(false, true);
}

SetKeysCommand::SetKeysCommand(CurveWidget *editor,
               const AddKeysCommand::KeysToAddList & keys,
               QUndoCommand *parent)
: AddKeysCommand(editor,keys,parent)
{
    
}

SetKeysCommand::SetKeysCommand(CurveWidget *editor,
               const boost::shared_ptr<CurveGui>& curve,
               const std::vector<KeyFrame> & keys,
               QUndoCommand *parent)
: AddKeysCommand(editor, curve, keys, parent)
, _guiCurve(curve)
, _oldCurve()
{
    boost::shared_ptr<Curve> internalCurve = curve->getInternalCurve();
    assert(internalCurve);
    _oldCurve.reset(new Curve(*internalCurve));
    
}

void
SetKeysCommand::undo()
{
    KnobCurveGui* isKnobCurve = dynamic_cast<KnobCurveGui*>(_guiCurve.get());
    BezierCPCurveGui* isBezierCurve = dynamic_cast<BezierCPCurveGui*>(_guiCurve.get());
    
    if (isKnobCurve) {
        KnobPtr knob = isKnobCurve->getInternalKnob();
        boost::shared_ptr<KnobParametric> isParametric = boost::dynamic_pointer_cast<KnobParametric>(knob);
        if (!isParametric) {
            knob->cloneCurve(ViewSpec::all(), isKnobCurve->getDimension(), *_oldCurve);
        } else {
            _guiCurve->getInternalCurve()->clone(*_oldCurve);
        }
    } else if (isBezierCurve) {
        // not implemented
    }
}

void
SetKeysCommand::redo()
{
    addOrRemoveKeyframe(true, true);
}

//////////////////////////////REMOVE  MULTIPLE KEYS COMMAND//////////////////////////////////////////////
RemoveKeysCommand::RemoveKeysCommand(CurveWidget* editor,
                                     const std::map<boost::shared_ptr<CurveGui> ,std::vector<KeyFrame> > & keys,
                                     QUndoCommand *parent )
    : QUndoCommand(parent)
      , _keys(keys)
      , _curveWidget(editor)
{
}

void
RemoveKeysCommand::addOrRemoveKeyframe(bool add)
{
    for (std::map<boost::shared_ptr<CurveGui> ,std::vector<KeyFrame> >::iterator it = _keys.begin(); it!=_keys.end(); ++it) {
        
        KnobCurveGui* isKnobCurve = dynamic_cast<KnobCurveGui*>(it->first.get());
        BezierCPCurveGui* isBezierCurve = dynamic_cast<BezierCPCurveGui*>(it->first.get());
        KnobGui* guiKnob = isKnobCurve ? isKnobCurve->getKnobGui() : 0;
        
        KnobPtr knob;
        
        if (isKnobCurve) {
            knob = isKnobCurve->getInternalKnob();
        }
        boost::shared_ptr<KnobParametric> isParametric;
        if (knob) {
            isParametric = boost::dynamic_pointer_cast<KnobParametric>(knob);
        }
        
        if (guiKnob && isKnobCurve && !isParametric) {
            if (add) {
                guiKnob->setKeyframes(it->second, isKnobCurve->getDimension(), ViewIdx(0) );
            } else {
                guiKnob->removeKeyframes(it->second, isKnobCurve->getDimension(), ViewIdx(0)  );
                
            }
        } else {
            
            for (std::size_t i = 0; i < it->second.size(); ++i) {
                if (isKnobCurve) {
                    
                    
                    isKnobCurve->getInternalKnob()->beginChanges();
                    
                    if (add) {
                        
                        int time = it->second[i].getTime();
                        
                        if (isParametric) {
                            
                            StatusEnum st = isParametric->addControlPoint( isKnobCurve->getDimension(), it->second[i].getTime(),it->second[i].getValue() );
                            assert(st == eStatusOK);
                            Q_UNUSED(st);
                        } else {
                            Knob<double>* isDouble = dynamic_cast<Knob<double>*>(knob.get());
                            Knob<bool>* isBool = dynamic_cast<Knob<bool>*>(knob.get());
                            Knob<int>* isInt = dynamic_cast<Knob<int>*>(knob.get());
                            Knob<std::string>* isString = dynamic_cast<Knob<std::string>*>(knob.get());
                            if (isDouble) {
                                isDouble->setValueAtTime(time, ViewIdx(0), isDouble->getValueAtTime(time, 0, ViewIdx(0)), isKnobCurve->getDimension());
                            } else if (isBool) {
                                isBool->setValueAtTime(time, ViewIdx(0), isBool->getValueAtTime(time, 0, ViewIdx(0)), isKnobCurve->getDimension());
                            } else if (isInt) {
                                isInt->setValueAtTime(time, ViewIdx(0), isInt->getValueAtTime(time,0, ViewIdx(0)), isKnobCurve->getDimension());
                            } else if (isString) {
                                isString->setValueAtTime(time, ViewIdx(0), isString->getValueAtTime(time,0, ViewIdx(0) ), isKnobCurve->getDimension());
                            }
                        }
                    } else {
                        
                        boost::shared_ptr<KnobParametric> knob = boost::dynamic_pointer_cast<KnobParametric>( isKnobCurve->getInternalKnob() );
                        
                        if (knob) {
                            StatusEnum st = knob->deleteControlPoint( isKnobCurve->getDimension(),
                                                                             it->first->getInternalCurve()->keyFrameIndex( it->second[i].getTime() ) );
                            assert(st == eStatusOK);
                            Q_UNUSED(st);
                        } else {
                            isKnobCurve->getInternalKnob()->deleteValueAtTime(eCurveChangeReasonCurveEditor, it->second[i].getTime(), ViewSpec::all(), isKnobCurve->getDimension() );
                        }
                    }
                } else if (isBezierCurve) {
                    boost::shared_ptr<Bezier> b = isBezierCurve->getBezier();
                    assert(b);
                    if (add) {
                        b->setKeyframe(it->second[i].getTime());
                    } else {
                        b->removeKeyframe(it->second[i].getTime());
                    }
                } // if (isKnobCurve) {
            } // for (std::size_t i = 0; i < it->second.size(); ++i) {
        } // if (guiKnob) {
        if (isKnobCurve) {
            isKnobCurve->getInternalKnob()->endChanges();
        }
    }

    _curveWidget->update();
    setText( QObject::tr("Remove multiple keyframes") );
}

void
RemoveKeysCommand::undo()
{
    addOrRemoveKeyframe(true);
}

void
RemoveKeysCommand::redo()
{
    addOrRemoveKeyframe(false);
}


//////////////////////////////MOVE MULTIPLE KEYS COMMAND//////////////////////////////////////////////
MoveKeysCommand::MoveKeysCommand(CurveWidget* widget,
                                 const SelectedKeys &keys,
                                 double dt,
                                 double dv,
                                 bool updateOnFirstRedo,
                                 QUndoCommand *parent )
    : QUndoCommand(parent)
      , _firstRedoCalled(false)
      , _updateOnFirstRedo(updateOnFirstRedo)
      , _dt(dt)
      , _dv(dv)
      , _keys(keys)
      , _widget(widget)
{

}

static void
moveKey(KeyPtr &k,
        double dt,
        double dv)
{
    
    
    KnobCurveGui* isKnobCurve = dynamic_cast<KnobCurveGui*>(k->curve.get());
    BezierCPCurveGui* isBezierCurve = dynamic_cast<BezierCPCurveGui*>(k->curve.get());
    if (isKnobCurve) {
        KnobPtr knob = isKnobCurve->getInternalKnob();
        KnobParametric* isParametric = dynamic_cast<KnobParametric*>(knob.get());
        
        if (isParametric) {
           // std::pair<double,double> curveYRange = k->curve->getInternalCurve()->getCurveYRange();
            double newX = k->key.getTime() + dt;
            double newY = k->key.getValue() + dv;
            boost::shared_ptr<Curve> curve = k->curve->getInternalCurve();
            
            if (curve->isYComponentMovable()) {
                if ( curve->areKeyFramesValuesClampedToIntegers() ) {
                    newY = std::floor(newY + 0.5);
                } else if ( curve->areKeyFramesValuesClampedToBooleans() ) {
                    newY = newY < 0.5 ? 0 : 1;
                }
            } else {
                newY = k->key.getValue();
            }
            double oldTime = k->key.getTime();
            int keyframeIndex = curve->keyFrameIndex(oldTime);
            int newIndex;
            
            k->key = curve->setKeyFrameValueAndTime(newX,newY, keyframeIndex, &newIndex);
            isParametric->evaluateValueChange(isKnobCurve->getDimension(), isParametric->getCurrentTime(), ViewIdx(0), eValueChangedReasonUserEdited);
        } else {
            knob->moveValueAtTime(eCurveChangeReasonCurveEditor, k->key.getTime(), ViewSpec::all(), isKnobCurve->getDimension(), dt, dv,&k->key);
        }
    } else if (isBezierCurve) {
        double oldTime = k->key.getTime();
        k->key.setTime(oldTime + dt);
        isBezierCurve->getBezier()->moveKeyframe(oldTime, k->key.getTime());

    }
}

void
MoveKeysCommand::move(double dt,
                      double dv)
{
    //Prevent all redraws from moveKey
    _widget->setUpdatesEnabled(false);
    
    std::list<KnobHolder*> differentKnobs;

    std::list<boost::shared_ptr<RotoContext> > rotoToEvaluate;
    
    for (SelectedKeys::iterator it = _keys.begin(); it != _keys.end(); ++it) {
        KnobCurveGui* isKnobCurve = dynamic_cast<KnobCurveGui*>((*it)->curve.get());
        if (isKnobCurve) {
            
            if (!isKnobCurve->getKnobGui()) {
                boost::shared_ptr<RotoContext> roto = isKnobCurve->getRotoContext();
                assert(roto);
                if (std::find(rotoToEvaluate.begin(),rotoToEvaluate.end(),roto) == rotoToEvaluate.end()) {
                    rotoToEvaluate.push_back(roto);
                }
            } else {
                KnobI* k = isKnobCurve->getInternalKnob().get();
                if (k->getHolder()) {
                    if ( std::find(differentKnobs.begin(), differentKnobs.end(), k->getHolder()) == differentKnobs.end() ) {
                        differentKnobs.push_back(k->getHolder());
                        k->getHolder()->beginChanges();
                    }
                }
            }
        }
    }
    
    
    for (SelectedKeys::iterator it = _keys.begin(); it != _keys.end(); ++it) {
        moveKey(*it, dt, dv);
    }
    
    if (_firstRedoCalled || _updateOnFirstRedo) {
        for (std::list<KnobHolder*>::iterator it = differentKnobs.begin(); it != differentKnobs.end(); ++it) {
            (*it)->endChanges();
        }
    }
    
    _widget->setUpdatesEnabled(true);
    
    for (std::list<boost::shared_ptr<RotoContext> >::iterator it = rotoToEvaluate.begin(); it != rotoToEvaluate.end(); ++it) {
        (*it)->evaluateChange();
    }

    _widget->refreshSelectedKeys();
}

void
MoveKeysCommand::undo()
{
    move(-_dt,-_dv);
    setText( QObject::tr("Move multiple keys") );
}

void
MoveKeysCommand::redo()
{
    move(_dt,_dv);
    _firstRedoCalled = true;
    setText( QObject::tr("Move multiple keys") );
}

bool
MoveKeysCommand::mergeWith(const QUndoCommand * command)
{
    const MoveKeysCommand* cmd = dynamic_cast<const MoveKeysCommand*>(command);

    if ( cmd && ( cmd->id() == id() ) ) {
        if ( cmd->_keys.size() != _keys.size() ) {
            return false;
        }

        SelectedKeys::const_iterator itother = cmd->_keys.begin();
        for (SelectedKeys::const_iterator it = _keys.begin(); it != _keys.end(); ++it, ++itother) {
            if (*itother != *it) {
                return false;
            }
        }

        _dt += cmd->_dt;
        _dv += cmd->_dv;

        return true;
    } else {
        return false;
    }
}

int
MoveKeysCommand::id() const
{
    return kCurveEditorMoveMultipleKeysCommandCompressionID;
}

//////////////////////////////SET MULTIPLE KEYS INTERPOLATION COMMAND//////////////////////////////////////////////
SetKeysInterpolationCommand::SetKeysInterpolationCommand(CurveWidget* widget,
                                                         const std::list< KeyInterpolationChange > & keys,
                                                         QUndoCommand *parent)
    : QUndoCommand(parent)
      , _keys(keys)
      , _widget(widget)
{
}

void
SetKeysInterpolationCommand::setNewInterpolation(bool undo)
{
    std::list<KnobI*> differentKnobs;

    std::list<boost::shared_ptr<RotoContext> > rotoToEvaluate;

    for (std::list< KeyInterpolationChange >::iterator it = _keys.begin(); it != _keys.end(); ++it) {
        KnobCurveGui* isKnobCurve = dynamic_cast<KnobCurveGui*>(it->key->curve.get());
        if (isKnobCurve) {
            
            if (!isKnobCurve->getKnobGui()) {
                boost::shared_ptr<RotoContext> roto = isKnobCurve->getRotoContext();
                assert(roto);
                if (std::find(rotoToEvaluate.begin(),rotoToEvaluate.end(),roto) == rotoToEvaluate.end()) {
                    rotoToEvaluate.push_back(roto);
                }
            } else {
                KnobI* k = isKnobCurve->getInternalKnob().get();
                if ( std::find(differentKnobs.begin(), differentKnobs.end(), k) == differentKnobs.end() ) {
                    differentKnobs.push_back(k);
                    k->beginChanges();
                }
            }
        } else {
            BezierCPCurveGui* bezierCurve = dynamic_cast<BezierCPCurveGui*>(it->key->curve.get());
            assert(bezierCurve);
            rotoToEvaluate.push_back(bezierCurve->getBezier()->getContext());
        }
    }

    for (std::list< KeyInterpolationChange >::iterator it = _keys.begin(); it != _keys.end(); ++it) {
        
        KeyframeTypeEnum interp = undo ? it->oldInterp : it->newInterp;
        
        KnobCurveGui* isKnobCurve = dynamic_cast<KnobCurveGui*>(it->key->curve.get());
        if (isKnobCurve) {
            KnobPtr knob = isKnobCurve->getInternalKnob();
            KnobParametric* isParametric = dynamic_cast<KnobParametric*>(knob.get());
            
            if (isParametric) {
                
                int keyframeIndex = it->key->curve->getKeyFrameIndex( it->key->key.getTime() );
                if (keyframeIndex != -1) {
                   it->key->key = it->key->curve->setKeyFrameInterpolation(interp, keyframeIndex);
                }
                isParametric->evaluateValueChange(isKnobCurve->getDimension(), it->key->key.getTime(), ViewIdx(0), eValueChangedReasonUserEdited);
            } else {
                knob->setInterpolationAtTime(eCurveChangeReasonCurveEditor, ViewIdx(0),  isKnobCurve->getDimension(), it->key->key.getTime(), interp, &it->key->key);
            }
        } else {
            ///interpolation for bezier curve is either linear or constant
            interp = interp == eKeyframeTypeConstant ? eKeyframeTypeConstant :
            eKeyframeTypeLinear;
            int keyframeIndex = it->key->curve->getKeyFrameIndex( it->key->key.getTime() );
            if (keyframeIndex != -1) {
                it->key->curve->setKeyFrameInterpolation(interp, keyframeIndex);
            }
        }
        
    }
    
    for (std::list<KnobI*>::iterator it = differentKnobs.begin(); it != differentKnobs.end(); ++it) {
        (*it)->endChanges();
    }
    for (std::list<boost::shared_ptr<RotoContext> >::iterator it = rotoToEvaluate.begin(); it != rotoToEvaluate.end(); ++it) {
        (*it)->evaluateChange();
    }

    _widget->refreshSelectedKeys();
    setText( QObject::tr("Set multiple keys interpolation") );
}

void
SetKeysInterpolationCommand::undo()
{
    setNewInterpolation(true);
}

void
SetKeysInterpolationCommand::redo()
{
    setNewInterpolation(false);
}

/////////////////////////// MoveTangentCommand

MoveTangentCommand::MoveTangentCommand(CurveWidget* widget,
                                       SelectedTangentEnum deriv,
                                       const KeyPtr& key,
                                       double dx,double dy, //< dx dy relative to the center of the keyframe
                                       bool updateOnFirstRedo,
                                       QUndoCommand *parent)
: QUndoCommand(parent)
, _widget(widget)
, _key(key)
, _deriv(deriv)
, _oldInterp(key->key.getInterpolation())
, _oldLeft(key->key.getLeftDerivative())
, _oldRight(key->key.getRightDerivative())
, _setBoth(false)
, _updateOnFirstRedo(updateOnFirstRedo)
, _firstRedoCalled(false)
{
    KeyFrameSet keys = key->curve->getInternalCurve()->getKeyFrames_mt_safe();
    KeyFrameSet::const_iterator cur = keys.find(key->key);
    assert( cur != keys.end() );

    //find next and previous keyframes
    KeyFrameSet::const_iterator prev = cur;
    if ( prev != keys.begin() ) {
        --prev;
    } else {
        prev = keys.end();
    }
    KeyFrameSet::const_iterator next = cur;
    if (next != keys.end()) {
        ++next;
    }

    // handle first and last keyframe correctly:
    // - if their interpolation was eKeyframeTypeCatmullRom or eKeyframeTypeCubic, then it becomes eKeyframeTypeFree
    // - in all other cases it becomes eKeyframeTypeBroken
    KeyframeTypeEnum interp = key->key.getInterpolation();
    bool keyframeIsFirstOrLast = ( prev == keys.end() || next == keys.end() );
    bool interpIsNotBroken = (interp != eKeyframeTypeBroken);
    bool interpIsCatmullRomOrCubicOrFree = (interp == eKeyframeTypeCatmullRom ||
                                            interp == eKeyframeTypeCubic ||
                                            interp == eKeyframeTypeFree);
    _setBoth = keyframeIsFirstOrLast ? interpIsCatmullRomOrCubicOrFree : interpIsNotBroken;

    bool isLeft;
    if (deriv == eSelectedTangentLeft) {
        //if dx is not of the good sign it would make the curve uncontrollable
        if (dx <= 0) {
            dx = 0.0001;
        }
        isLeft = true;
    } else {
        //if dx is not of the good sign it would make the curve uncontrollable
        if (dx >= 0) {
            dx = -0.0001;
        }
        isLeft = false;
    }
    double derivative = dy / dx;
    
    if (_setBoth) {
        _newInterp = eKeyframeTypeFree;
        _newLeft = derivative;
        _newRight = derivative;
    } else {
        if (isLeft) {
            _newLeft = derivative;
            _newRight = _oldRight;
        } else {
            _newLeft = _oldLeft;
            _newRight = derivative;
        }
        _newInterp = eKeyframeTypeBroken;
    }
}

MoveTangentCommand::MoveTangentCommand(CurveWidget* widget,
                   SelectedTangentEnum deriv,
                   const KeyPtr& key,
                   double derivative,
                   QUndoCommand *parent)
: QUndoCommand(parent)
, _widget(widget)
, _key(key)
, _deriv(deriv)
, _oldInterp(key->key.getInterpolation())
, _oldLeft(key->key.getLeftDerivative())
, _oldRight(key->key.getRightDerivative())
, _setBoth(true)
, _updateOnFirstRedo(true)
, _firstRedoCalled(false)
{
    _newInterp = _oldInterp == eKeyframeTypeBroken ? eKeyframeTypeBroken : eKeyframeTypeFree;
    _setBoth = _newInterp == eKeyframeTypeFree;
    
    switch (deriv) {
        case eSelectedTangentLeft:
            _newLeft = derivative;
            if (_newInterp == eKeyframeTypeBroken) {
                _newRight = _oldRight;
            } else {
                _newRight = derivative;
            }
            break;
        case eSelectedTangentRight:
            _newRight = derivative;
            if (_newInterp == eKeyframeTypeBroken) {
                _newLeft = _oldLeft;
            } else {
                _newLeft = derivative;
            }
        default:
            break;
    }
}


void
MoveTangentCommand::setNewDerivatives(bool undo)
{
    KnobCurveGui* isKnobCurve = dynamic_cast<KnobCurveGui*>(_key->curve.get());
    if (isKnobCurve) {
        KnobPtr attachedKnob = isKnobCurve->getInternalKnob();
        assert(attachedKnob);
        KnobParametric* isParametric = dynamic_cast<KnobParametric*>(attachedKnob.get());
        
        
        double left = undo ? _oldLeft : _newLeft;
        double right = undo ? _oldRight : _newRight;
        KeyframeTypeEnum interp = undo ? _oldInterp : _newInterp;
        
        if (!isParametric) {
            attachedKnob->beginChanges();
            if (_setBoth) {
                attachedKnob->moveDerivativesAtTime(eCurveChangeReasonCurveEditor, ViewIdx(0), isKnobCurve->getDimension(), _key->key.getTime(), left, right);
            } else {
                attachedKnob->moveDerivativeAtTime(eCurveChangeReasonCurveEditor, ViewIdx(0), isKnobCurve->getDimension(), _key->key.getTime(),
                                                   _deriv == eSelectedTangentLeft ? left : right,
                                                   _deriv == eSelectedTangentLeft);
                
            }
            attachedKnob->setInterpolationAtTime(eCurveChangeReasonCurveEditor, ViewIdx(0), isKnobCurve->getDimension(), _key->key.getTime(), interp, &_key->key);
            if (_firstRedoCalled || _updateOnFirstRedo) {
                attachedKnob->endChanges();
            }
        } else {
            int keyframeIndexInCurve = _key->curve->getInternalCurve()->keyFrameIndex( _key->key.getTime() );
            _key->key = _key->curve->getInternalCurve()->setKeyFrameInterpolation(interp, keyframeIndexInCurve);
            _key->key = _key->curve->getInternalCurve()->setKeyFrameDerivatives(left, right,keyframeIndexInCurve);
            attachedKnob->evaluateValueChange(isKnobCurve->getDimension(), _key->key.getTime(), ViewIdx(0),  eValueChangedReasonUserEdited);
        }
        
        _widget->refreshDisplayedTangents();
    }
}


void
MoveTangentCommand::undo()
{
    setNewDerivatives(true);
    setText( QObject::tr("Move keyframe slope") );
}


void
MoveTangentCommand::redo()
{
    setNewDerivatives(false);
    _firstRedoCalled = true;
    setText( QObject::tr("Move keyframe slope") );
}

int
MoveTangentCommand::id() const
{
    return kCurveEditorMoveTangentsCommandCompressionID;
}

bool
MoveTangentCommand::mergeWith(const QUndoCommand * command)
{
    const MoveTangentCommand* cmd = dynamic_cast<const MoveTangentCommand*>(command);
    
    if ( cmd && ( cmd->id() == id() ) ) {
        if ( cmd->_key != _key ) {
            return false;
        }
        
        
        _newInterp = cmd->_newInterp;
        _newLeft = cmd->_newLeft;
        _newRight = cmd->_newRight;
        
        return true;
    } else {
        return false;
    }
}



TransformKeysCommand::TransformKeysCommand(CurveWidget* widget,
                     const SelectedKeys & keys,
                     double centerX,
                     double centerY,
                     double tx,
                     double ty,
                     double sx,
                     double sy,
                     bool updateOnFirstRedo,
                     QUndoCommand *parent)
: QUndoCommand(parent)
, _firstRedoCalled(false)
, _updateOnFirstRedo(updateOnFirstRedo)
, _keys(keys)
, _widget(widget)
, _matrix(new Transform::Matrix3x3)
{
    *_matrix = Transform::matTransformCanonical(tx, ty, sx, sy, 0, 0, true, 0, centerX, centerY);
}

TransformKeysCommand::~TransformKeysCommand()
{
}


void
TransformKeysCommand::undo()
{
    std::list<KnobHolder*> differentKnobs;
    
    std::list<boost::shared_ptr<RotoContext> > rotoToEvaluate;
    
    std::list<CurveGui*> processedCurves;
    for (SelectedKeys::iterator it = _keys.begin(); it != _keys.end(); ++it) {
        KnobCurveGui* isKnobCurve = dynamic_cast<KnobCurveGui*>((*it)->curve.get());
        if (isKnobCurve) {
            
            if (!isKnobCurve->getKnobGui()) {
                boost::shared_ptr<RotoContext> roto = isKnobCurve->getRotoContext();
                assert(roto);
                if (std::find(rotoToEvaluate.begin(),rotoToEvaluate.end(),roto) == rotoToEvaluate.end()) {
                    rotoToEvaluate.push_back(roto);
                }
            } else {
                KnobI* k = isKnobCurve->getInternalKnob().get();
                if (k->getHolder()) {
                    if ( std::find(differentKnobs.begin(), differentKnobs.end(), k->getHolder()) == differentKnobs.end() ) {
                        differentKnobs.push_back(k->getHolder());
                        k->getHolder()->beginChanges();
                    }
                }
            }
        }
        if (std::find(processedCurves.begin(), processedCurves.end(), (*it)->curve.get()) == processedCurves.end()) {
            processedCurves.push_back((*it)->curve.get());
        }
        
    }
    
    for (std::list<CurveCopy>::iterator it = _curves.begin();
         it != _curves.end(); ++it) {
        KnobCurveGui* isKnobCurve = dynamic_cast<KnobCurveGui*>(it->guiCurve);
        if (isKnobCurve && !dynamic_cast<KnobParametric*>(isKnobCurve->getInternalKnob().get())) {
            isKnobCurve->getInternalKnob()->cloneCurve(ViewSpec::all(), isKnobCurve->getDimension(), *it->oldCpy);
        } else {
            it->original->clone(*it->oldCpy);
        }
    }
    for (std::list<BezierCopy>::iterator it = _beziers.begin();
         it != _beziers.end(); ++it) {
        it->original->clone(it->oldCpy.get());
    }
    
        
    for (std::list<KnobHolder*>::iterator it = differentKnobs.begin(); it != differentKnobs.end(); ++it) {
        (*it)->endChanges();
    }
    
    for (std::list<boost::shared_ptr<RotoContext> >::iterator it = rotoToEvaluate.begin(); it != rotoToEvaluate.end(); ++it) {
        (*it)->evaluateChange();
    }
    
    _widget->setSelectedKeys(_altKeys);
}

void
TransformKeysCommand::redo()
{
    
    std::list<KnobHolder*> differentKnobs;
    
    std::list<boost::shared_ptr<RotoContext> > rotoToEvaluate;
    
    std::list<CurveGui*> processedCurves;
    for (SelectedKeys::iterator it = _keys.begin(); it != _keys.end(); ++it) {
        KnobCurveGui* isKnobCurve = dynamic_cast<KnobCurveGui*>((*it)->curve.get());
        if (isKnobCurve) {
            
            if (!isKnobCurve->getKnobGui()) {
                boost::shared_ptr<RotoContext> roto = isKnobCurve->getRotoContext();
                assert(roto);
                if (std::find(rotoToEvaluate.begin(),rotoToEvaluate.end(),roto) == rotoToEvaluate.end()) {
                    rotoToEvaluate.push_back(roto);
                }
            } else {
                KnobI* k = isKnobCurve->getInternalKnob().get();
                if (k->getHolder()) {
                    if ( std::find(differentKnobs.begin(), differentKnobs.end(), k->getHolder()) == differentKnobs.end() ) {
                        differentKnobs.push_back(k->getHolder());
                        k->getHolder()->beginChanges();
                    }
                }
            }
        }
        if (std::find(processedCurves.begin(), processedCurves.end(), (*it)->curve.get()) == processedCurves.end()) {
            processedCurves.push_back((*it)->curve.get());
        }
        
    }
    
    if (!_firstRedoCalled) {
        for (std::list<CurveGui*>::iterator it = processedCurves.begin(); it != processedCurves.end(); ++it) {
            boost::shared_ptr<Curve> internalCurve = (*it)->getInternalCurve();
            if (internalCurve) {
                CurveCopy c;
                c.guiCurve = *it;
                c.oldCpy.reset(new Curve(*internalCurve));
                c.original = internalCurve;
                _curves.push_back(c);
            } else {
                BezierCPCurveGui* isBezier = dynamic_cast<BezierCPCurveGui*>(*it);
                assert(isBezier);
                if (isBezier) {
                    BezierCopy b;
                    b.guiCurve = *it;
                    b.original = isBezier->getBezier();
                    b.oldCpy.reset(new Bezier(b.original->getContext(),b.original->getScriptName(),b.original->getParentLayer(),b.original->isOpenBezier()));
                    b.oldCpy->clone(b.original.get());
                    _beziers.push_back(b);
                }
            }
        }
       
 
        
    } else {
        _keys = _altKeys;
    }
    
    _altKeys.clear();
    for (SelectedKeys::reverse_iterator it = _keys.rbegin(); it != _keys.rend(); ++it) {
        boost::shared_ptr<SelectedKey> oldKey(new SelectedKey);
        oldKey->curve = (*it)->curve;
        oldKey->key =  (*it)->key;
        oldKey->leftTan = (*it)->leftTan;
        oldKey->rightTan = (*it)->rightTan;
        transform(*it);
        _altKeys.push_back(oldKey);
    }
    
    
    
    if (_firstRedoCalled || _updateOnFirstRedo) {
        for (std::list<KnobHolder*>::iterator it = differentKnobs.begin(); it != differentKnobs.end(); ++it) {
            (*it)->endChanges();
        }
    }
    
    for (std::list<boost::shared_ptr<RotoContext> >::iterator it = rotoToEvaluate.begin(); it != rotoToEvaluate.end(); ++it) {
        (*it)->evaluateChange();
    }

    _widget->setSelectedKeys(_keys);
    _firstRedoCalled = true;
    
}

int
TransformKeysCommand::id() const
{
    return kCurveEditorTransformKeysCommandCompressionID;
}

bool
TransformKeysCommand::mergeWith(const QUndoCommand * command)
{
    const TransformKeysCommand* cmd = dynamic_cast<const TransformKeysCommand*>(command);
    
    if ( cmd && ( cmd->id() == id() ) ) {
        if ( cmd->_keys.size() != _keys.size() ) {
            return false;
        }
        
        SelectedKeys::const_iterator itother = cmd->_keys.begin();
        for (SelectedKeys::const_iterator it = _keys.begin(); it != _keys.end(); ++it, ++itother) {
            if (*itother != *it) {
                return false;
            }
        }
        
        *_matrix = matMul(*_matrix, *cmd->_matrix);
        return true;
    } else {
        return false;
    }
}

void
TransformKeysCommand::transform(const KeyPtr& k)
{
    KnobCurveGui* isKnobCurve = dynamic_cast<KnobCurveGui*>(k->curve.get());
    BezierCPCurveGui* isBezierCurve = dynamic_cast<BezierCPCurveGui*>(k->curve.get());
    
    if (isKnobCurve) {
        KnobPtr knob = isKnobCurve->getInternalKnob();
        KnobParametric* isParametric = dynamic_cast<KnobParametric*>(knob.get());
        
        if (isParametric) {
            // std::pair<double,double> curveYRange = k->curve->getInternalCurve()->getCurveYRange();
            Transform::Point3D p;
            p.x = k->key.getTime();
            p.y = k->key.getValue();
            p.z = 1;
            p = Transform::matApply(*_matrix, p);
            
            boost::shared_ptr<Curve> curve = k->curve->getInternalCurve();
            if (curve && curve->isYComponentMovable()) {
                if ( curve->areKeyFramesValuesClampedToIntegers() ) {
                    p.y = std::floor(p.y + 0.5);
                } else if ( curve->areKeyFramesValuesClampedToBooleans() ) {
                    p.y = p.y < 0.5 ? 0 : 1;
                }
            } else {
                p.y = k->key.getValue();
            }
            double oldTime = k->key.getTime();
            int keyframeIndex = curve->keyFrameIndex(oldTime);
            int newIndex;
            
            k->key = curve->setKeyFrameValueAndTime(p.x,p.y, keyframeIndex, &newIndex);
            isParametric->evaluateValueChange(isKnobCurve->getDimension(), isParametric->getCurrentTime(), ViewIdx(0), eValueChangedReasonUserEdited);
        } else {
            knob->transformValueAtTime(eCurveChangeReasonCurveEditor, k->key.getTime(), ViewIdx(0), isKnobCurve->getDimension(), *_matrix,&k->key);
        }
    } else if (isBezierCurve) {
        double oldTime = k->key.getTime();
        Transform::Point3D p;
        p.x = oldTime;
        p.y = k->key.getValue();
        p.z = 1;
    
        p = Transform::matApply(*_matrix, p);
        k->key.setTime(p.x);
   
        isBezierCurve->getBezier()->moveKeyframe(oldTime, p.x);
    }
}

NATRON_NAMESPACE_EXIT;
