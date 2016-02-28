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

#ifndef NATRON_GUI_KNOBUNDOCOMMAND_H
#define NATRON_GUI_KNOBUNDOCOMMAND_H

// ***** BEGIN PYTHON BLOCK *****
// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>
// ***** END PYTHON BLOCK *****

#include "Global/Macros.h"

#include <map>
#include <vector>

CLANG_DIAG_OFF(deprecated)
CLANG_DIAG_OFF(uninitialized)
#include <QUndoCommand>
CLANG_DIAG_ON(deprecated)
CLANG_DIAG_ON(uninitialized)

#include "Global/GlobalDefines.h"

#include "Engine/Variant.h"
#include "Engine/Curve.h"
#include "Engine/TimeLine.h"
#include "Engine/Curve.h"
#include "Engine/Knob.h"
#include "Engine/EffectInstance.h"
#include "Engine/ViewIdx.h"

#include "Gui/KnobGui.h"
#include "Gui/Gui.h"
#include "Gui/CurveEditor.h"
#include "Gui/CurveWidget.h"
#include "Gui/GuiAppInstance.h"
#include "Gui/GuiFwd.h"

NATRON_NAMESPACE_ENTER;

//================================================================

template<typename T>
class KnobUndoCommand
    : public QUndoCommand
{
public:

    KnobUndoCommand(const KnobGuiPtr& knob,
                    const T &oldValue,
                    const T &newValue,
                    int dimension = 0,
                    bool refreshGui = true,
                    QUndoCommand *parent = 0)
        : QUndoCommand(parent)
          , _dimension(dimension)
          , _oldValue()
          , _newValue()
          , _knob(knob)
          , _valueChangedReturnCode(1)
          , _newKeys(1)
          , _oldKeys(1)
          , _merge(true)
          , _refreshGuiFirstTime(refreshGui)
          , _firstRedoCalled(false)
    {
        _oldValue.push_back(oldValue);
        _newValue.push_back(newValue);
    }

    ///We moved from std::vector to std::list instead because std::vector<bool> expands to a bit field(std::_Bit_reference)
    ///and produces an unresolved symbol error.
    KnobUndoCommand(const KnobGuiPtr& knob,
                    const std::list<T> &oldValue,
                    const std::list<T> &newValue,
                    bool refreshGui = true,
                    QUndoCommand *parent = 0)
        : QUndoCommand(parent)
          , _dimension(-1)
          , _oldValue(oldValue)
          , _newValue(newValue)
          , _knob(knob)
          , _valueChangedReturnCode( oldValue.size() )
          , _newKeys( oldValue.size() )
          , _oldKeys( oldValue.size() )
          , _merge(true)
          , _refreshGuiFirstTime(refreshGui)
          , _firstRedoCalled(false)
    {
    }

    virtual ~KnobUndoCommand() OVERRIDE
    {
    }

private:
    virtual void undo() OVERRIDE FINAL
    {
        bool modifiedKeyFrame = false;
        int i = 0;

        KnobPtr knob = _knob->getKnob();
        knob->beginChanges();
        
        assert((int)_oldValue.size() == _knob->getKnob()->getDimension() || _dimension != -1);
        
        typename std::list<T>::iterator next = _oldValue.end();
        if (next != _oldValue.end()) {
            ++next;
        }
        
        for (typename std::list<T>::iterator it = _oldValue.begin(); it != _oldValue.end(); ++it) {
            int dimension = _dimension == -1 ? i : _dimension;
          
            if (it == _oldValue.begin() && _oldValue.size() > 1) {
                knob->blockValueChanges();
            }
            
            if (next == _oldValue.end() && _oldValue.size() > 1) {
                knob->unblockValueChanges();
            }

            
            _knob->setValue(dimension,*it,NULL,false,eValueChangedReasonUserEdited);
            
            if (knob->getHolder()->getApp() ) {
                if (_valueChangedReturnCode[i] == 1) { //the value change also added a keyframe
                    _knob->removeKeyFrame(_newKeys[i].getTime(),dimension, ViewIdx(0));
                    modifiedKeyFrame = true;
                } else if (_valueChangedReturnCode[i] == 2) {
                    //the value change moved a keyframe
                    _knob->removeKeyFrame(_newKeys[i].getTime(),dimension, ViewIdx(0));
                    _knob->setKeyframe(_oldKeys[i].getTime(), dimension, ViewIdx(0));
                    modifiedKeyFrame = true;
                }
            }

            if (next != _oldValue.end()) {
                ++next;
            }
            ++i;
    
        }
        
        ///This will refresh all dimensions
        _knob->onInternalValueChanged(ViewSpec::all(), -1, eValueChangedReasonNatronGuiEdited);
        
        knob->endChanges();
        if (modifiedKeyFrame) {
            _knob->getGui()->getCurveEditor()->getCurveWidget()->refreshSelectedKeysAndUpdate();
        }

        setText( QObject::tr("Set value of %1")
                 .arg( _knob->getKnob()->getLabel().c_str() ) );
    }

    virtual void redo() OVERRIDE FINAL
    {
        double time = 0;
        
        KnobPtr knob = _knob->getKnob();
        if ( knob->getHolder() && knob->getHolder()->getApp() ) {
            time = knob->getHolder()->getApp()->getTimeLine()->currentFrame();
        }

        assert((int)_oldValue.size() == _knob->getKnob()->getDimension() || _dimension != -1);
        
        bool modifiedKeyFrames = false;

        knob->beginChanges();
        int i = 0;
        typename std::list<T>::iterator next = _newValue.end();
        if (next != _newValue.end()) {
            ++next;
        }
        for (typename std::list<T>::iterator it = _newValue.begin(); it != _newValue.end(); ++it) {
            int dimension = _dimension == -1 ? i : _dimension;
    
            if (it == _newValue.begin() && _newValue.size() > 1) {
                knob->blockValueChanges();
            }
            
            boost::shared_ptr<Curve> c = knob->getCurve(ViewIdx(0),dimension);
            //find out if there's already an existing keyframe before calling setValue
            if (c) {
                bool found = c->getKeyFrameWithTime(time, &_oldKeys[i]);
                Q_UNUSED(found); // we don't care if it existed or not
            }

            if (next == _newValue.end() && _newValue.size() > 1) {
                knob->unblockValueChanges();
            }
            
            _valueChangedReturnCode[i] = _knob->setValue(dimension, *it, &_newKeys[i], false, eValueChangedReasonUserEdited);
            if (_valueChangedReturnCode[i] != KnobHelper::eValueChangedReturnCodeNoKeyframeAdded) {
                modifiedKeyFrames = true;
            }
    
            ///if we added a keyframe, prevent this command to merge with any other command
            if (_valueChangedReturnCode[i] == KnobHelper::eValueChangedReturnCodeKeyframeAdded) {
                _merge = false;
            }
            ++i;
            if (next != _newValue.end()) {
                ++next;
            }
        
        }
        
        
        ///This will refresh all dimensions
        if (_firstRedoCalled || _refreshGuiFirstTime) {
            _knob->onInternalValueChanged(ViewSpec::all(), -1, eValueChangedReasonNatronGuiEdited);
        }

        
        knob->endChanges();

        if (modifiedKeyFrames) {
            _knob->getGui()->getCurveEditor()->getCurveWidget()->refreshSelectedKeysAndUpdate();
        }

        setText( QObject::tr("Set value of %1")
                 .arg( knob->getLabel().c_str() ) );

        _firstRedoCalled = true;
    } // redo

    virtual int id() const OVERRIDE FINAL
    {
        return kKnobUndoChangeCommandCompressionID;
    }

    virtual bool mergeWith(const QUndoCommand *command) OVERRIDE FINAL
    {
        const KnobUndoCommand *knobCommand = dynamic_cast<const KnobUndoCommand *>(command);

        if ( !knobCommand || ( command->id() != id() ) ) {
            return false;
        }

        KnobGuiPtr knob = knobCommand->_knob;
        if ( (_knob != knob) || !_merge || !knobCommand->_merge || (_dimension != knobCommand->_dimension) ) {
            return false;
        }


        _newValue = knobCommand->_newValue;

        return true;
    }

private:
    // TODO: PIMPL
    int _dimension;
    std::list<T> _oldValue;
    std::list<T> _newValue;
    KnobGuiPtr _knob;
    std::vector<int> _valueChangedReturnCode;
    std::vector<KeyFrame> _newKeys;
    std::vector<KeyFrame>  _oldKeys;
    bool _merge;
    bool _refreshGuiFirstTime;
    bool _firstRedoCalled;
};


/**
 * @brief This class is used by the internal knob when it wants to group multiple edits into a single undo/redo action.
 * It is not used by the GUI
 **/
class MultipleKnobEditsUndoCommand
    : public QUndoCommand
{
    struct ValueToSet
    {
        Variant newValue,oldValue;
        int dimension;
        double time;
        bool setKeyFrame;
        int setValueRetCode;
    };

    ///For each knob, the second member points to a clone of the knob before the first redo() call was made
    typedef std::map < KnobGuiPtr, std::list<ValueToSet> >  ParamsMap;
    ParamsMap knobs;
    bool createNew;
    bool firstRedoCalled;
    ValueChangedReasonEnum _reason;
public:

    /**
     * @brief Make a new command
     * @param createNew If true this command will not merge with a previous same command
     * @param setKeyFrame if true, the command will use setValueAtTime instead of setValue in the redo() command.
     **/
    MultipleKnobEditsUndoCommand(const KnobGuiPtr& knob,
                                 ValueChangedReasonEnum reason,
                                 bool createNew,
                                 bool setKeyFrame,
                                 const Variant & value,
                                 int dimension,
                                 double time);

    virtual ~MultipleKnobEditsUndoCommand();

    virtual void undo() OVERRIDE FINAL;
    virtual void redo() OVERRIDE FINAL;
    virtual int id() const OVERRIDE FINAL;
    virtual bool mergeWith(const QUndoCommand *command) OVERRIDE FINAL;
    static KnobPtr createCopyForKnob(const KnobPtr & originalKnob);
};

struct PasteUndoCommandPrivate;
class PasteUndoCommand
    : public QUndoCommand
{
    
    boost::scoped_ptr<PasteUndoCommandPrivate> _imp;
public:

    PasteUndoCommand(const KnobGuiPtr& knob,
                     KnobClipBoardType type,
                     int fromDimension,
                     int targetDimension,
                     const KnobPtr& fromKnob);

    virtual ~PasteUndoCommand();

    virtual void undo() OVERRIDE FINAL;
    virtual void redo() OVERRIDE FINAL;
    
    void copyFrom(const KnobPtr& fromKnob, bool isRedo);
};


class RestoreDefaultsCommand
    : public QUndoCommand
{
public:

    RestoreDefaultsCommand(bool isNodeReset,
                           const std::list<KnobPtr > & knobs,
                           int targetDim,
                           QUndoCommand *parent = 0);
    virtual void undo();
    virtual void redo();

private:

    bool _isNodeReset;
    int _targetDim;
    std::list<KnobPtr > _knobs,_clones;
};

class SetExpressionCommand
: public QUndoCommand
{
public:
    
    SetExpressionCommand(const KnobPtr & knob,
                         bool hasRetVar,
                         int dimension,
                         const std::string& expr,
                         QUndoCommand *parent = 0);
    virtual void undo();
    virtual void redo();
    
private:
    
    boost::shared_ptr<KnobI > _knob;
    std::vector<std::string> _oldExprs;
    std::vector<bool> _hadRetVar;
    std::string _newExpr;
    bool _hasRetVar;
    int _dimension;
};

NATRON_NAMESPACE_EXIT;

#endif // NATRON_GUI_KNOBUNDOCOMMAND_H
