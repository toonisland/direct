from ShowBaseGlobal import *
import GuiManager
import GuiFrame
import Vec3

guiMgr = GuiManager.GuiManager.getPtr(base.win, base.mak.node())
font = (loader.loadModelOnce("phase_3/models/fonts/ttf-comic")).node()

class Frame:

    # special methods
    def __init__(self, name):
        self.name = name
        self.managed = 0
        self.offset = 0
        self.frame = GuiFrame.GuiFrame(name)
        self.items = []

    def __del__(self):
        if (self.managed):
            self.frame.unmanage()
        del(self.frame)
        
    def __str__(self):
        return "Frame: %s = %s" % (self.name, self.items)

    # accessing
    def getName(self):
        return self.name
    
    def setPos(self, x, y):
        v3 = Vec3.Vec3(x, 0., y)
        self.frame.setPos(v3)

    def setScale(self, scale):
        self.frame.setScale(scale)

    def getOffset(self):
        return self.offset

    def setOffset(self, offset):
        self.offset = offset

    # actions
    def manage(self):
        self.frame.manage(guiMgr, base.eventMgr.eventHandler)
        self.managed = 1
        
    def unmanage(self):
        self.frame.unmanage()
        self.managed = 0

    def recompute(self):
        self.frame.recompute()

    def clearAllPacking(self):
        self.frame.clearAllPacking()
        
    # content functions
    def addItem(self, item):
        self.frame.addItem(item.getGuiItem())
        self.items.append(item)

    def removeItem(self, item):
        self.frame.removeItem(item.getGuiItem())
        self.items.remove(item)
        
    def getItems(self):
        return self.items

    def printItems(self):
        print "frame items: %s" % (self.items)
        
    def packItem(self, item, relation, otherItem):
        if (item in self.items) and (otherItem in self.items):
            self.frame.packItem(item.getGuiItem(), relation,
                                otherItem.getGuiItem(), self.offset)
        else:
            print "warning: tried to pack item that aren't in frame"
            
    # convenience functions
    def makeVertical(self):
        # remove any previous packing
        #self.frame.clearAllPacking()
        # make each item (except first) align under the last
        for itemNum in range(1, len(self.items)):            
            self.packItem(self.items[itemNum], GuiFrame.GuiFrame.UNDER,
                          self.items[itemNum - 1])
            self.packItem(self.items[itemNum], GuiFrame.GuiFrame.ALIGNLEFT,
                          self.items[itemNum - 1])
        self.frame.recompute()
            
    def makeHorizontal(self):
        # remove any previous packing
        #self.frame.clearAllPacking()
        # make each item (except first) align right of the last
        for itemNum in range(1, len(self.items)):
            self.packItem(self.items[itemNum], GuiFrame.GuiFrame.RIGHT,
                          self.items[itemNum - 1])
            self.packItem(self.items[itemNum], GuiFrame.GuiFrame.ALIGNABOVE,
                          self.items[itemNum - 1])
        self.frame.recompute()
            
    def makeWideAsWidest(self):
        # make all the buttons as wide as the widest button in
        # the frame
        widest = 0
        widestWidth = 0.0
        # find the widest
        for item in self.items:
            thisWidth = item.getWidth()
            if (thisWidth > widestWidth):
                widest = self.items.index(item)
                widestWidth = thisWidth

        # make them all this wide
        for item in self.items:
            item.setWidth(widestWidth)

            
        
