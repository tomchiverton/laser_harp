#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
    
    
// print the available output ports to the console
midiOut.listPorts(); // via instance
//ofxMidiOut::listPorts(); // via static too

// connect
midiOut.openPort(0); // by number
    	
    
    ofBackground(100,100,100);
	sender.setup( HOST, PORT1 );
    myTracker.setup();
    myBeam = new beam*[NUM_BEAMS];
    
    trig_thresh = 4; // [frames/fps]
    dist_thresh = 205; // [pixels] threshold of pixels from contour centre to beam centre
    pixels = 20; // [pixels] min number of pixels for contour detection
    
    
    //load laser config points from xml file
    //if file doesn't exit, create it in bin/data folder
    ofPoint pLow;
    ofPoint pHigh;
	if( XML.loadFile("config_points.xml") ){
		XML.pushTag("config_points");
		for(int i=0; i<NUM_BEAMS; i++){
			XML.pushTag("beam",i);
            pLow = ofPoint(XML.getValue("x_l",0.0),XML.getValue("y_l",0.0));
            pHigh = ofPoint(XML.getValue("x_h",0.0),XML.getValue("y_h",0.0));
            myBeam[i] = new beam(pLow, pHigh, trig_thresh, dist_thresh);
			XML.popTag();
		}
	}else{
		XML.addTag("config_points");
		XML.pushTag("config_points");
		for(int i=0; i<NUM_BEAMS; i++){
            myBeam[i] = new beam(ofPoint(0,0), ofPoint(0,0), trig_thresh, dist_thresh);
			XML.addTag("beam");
			XML.pushTag("beam",i);
            XML.addValue("x_l",0);
            XML.addValue("y_l",0);
            XML.addValue("x_h",0);
            XML.addValue("y_h",0);
			XML.popTag();
		} XML.saveFile("config_points.xml");
	}
}

//--------------------------------------------------------------
void testApp::update(){
	
    myTracker.update();
    
    int beam_1_note=66;
    
    if(myTracker.bNewFrame){
			
		// find contours which are between the size of 10 pixels and 1/8 the w*h pixels.
		// find holes is set to false so we will get no interior contours
		contourFinder.findContours(myTracker.binaryOut, pixels, (340*240)/8, 10, false);	// find holes
        
        ofxOscMessage m;
        
        for(int i = 0; i < NUM_BEAMS; i++) myBeam[i]->reset();
		if (contourFinder.nBlobs == 0){
            
            for(int i = 0; i < NUM_BEAMS; i++) m.addIntArg(0);
//cout << " b " << contourFinder.nBlobs;
			//for(int i = 0; i < NUM_BEAMS; i++) m.addIntArg((int)myBeam[i]->isBeamOn());
            //for(int i = 0; i < NUM_BEAMS; i++) m.addFloatArg(myBeam[i]->beamHeight());

//out << "clear \n";            
                        midiOut.sendNoteOff(1, beam_1_note, 64);
			sender.sendMessage( m );
			m.clear();
		}else{
            
            float min_dist = dist_thresh;
            float current_dist;
            int beam;
            bool bSet = false;
            for (int i = 0; i < contourFinder.nBlobs; i++){
                currentPoint = contourFinder.blobs[i].centroid;
                for(int j = 0; j < NUM_BEAMS; j++){
                    current_dist = myBeam[j]->distance(currentPoint);
//cout << "X" << i << " j " << j << " dist " << current_dist << " from " << min_dist;
                    if(current_dist < min_dist){
                        min_dist = current_dist;
                        beam = j;
                        bSet = true;
                    } 
                }
                if(bSet) myBeam[beam]->set();
                bSet = false;
                min_dist = dist_thresh;
            }  
            
            
//cout << "beam 0 on " << myBeam[0]->isBeamOn()     <<   " h " <<      myBeam[0]->beamHeight() << " ";
            for(int i = 0; i < NUM_BEAMS; i++) m.addIntArg((int)myBeam[i]->isBeamOn());
            for(int i = 0; i < NUM_BEAMS; i++) m.addFloatArg(myBeam[i]->beamHeight());
            sender.sendMessage(m);
//cout << "send \n";            
            
            if( myBeam[0]->beamHeight() != 1 && myBeam[0]->beamHeight() != 0){
                midiOut.sendNoteOn(1, beam_1_note, 64 );//ofMap( myBeam[0]->beamHeight(), 0.1,1, 1,127, true ) );
                midiOut.sendPitchBend(1, ofMap( myBeam[0]->beamHeight(), 0.1,1, 0, MIDI_MAX_BEND) );
            }else{
                midiOut.sendNoteOff(1, beam_1_note, 64 );
            }
                
            
            m.clear();
	    }
	}
}

//--------------------------------------------------------------
void testApp::draw(){
	
    myTracker.draw();
    
    char centroidStr[1024];
	char areaStr[1024];
    char reportLaser[1024];
	
    for (int i = 0; i < contourFinder.nBlobs; i++){
        contourFinder.blobs[i].draw(700,310);
        /*
		sprintf(centroidStr, "centroid: x(%f) y(%f)\n", contourFinder.blobs[i].centroid.x, contourFinder.blobs[i].centroid.y);
		ofDrawBitmapString(centroidStr,20,630+10*i);
		sprintf(areaStr, "area: %f\n", contourFinder.blobs[i].area);
		ofDrawBitmapString(areaStr,20,640+contourFinder.nBlobs*10+10*i);
         */
    }
    
    
    ofSetHexColor(0xffffff);
    for(int i = 0; i < NUM_BEAMS; i++){
        if (myBeam[i]->isBeamOn()) {
            sprintf(reportLaser, "%i: %f\n", i+1, myBeam[i]->beamHeight());
            ofDrawBitmapString(reportLaser,360, 310+i*40);
        }
    }
    
	char reportStr[1024];
	sprintf(reportStr, "time threshold: %i (press: t/T)\ndistance threshold: %f (press d/D)\npixel threshold: %i (press p/P)\n1 set low, SHIFT-1 set high", trig_thresh, dist_thresh, pixels);
	ofDrawBitmapString(reportStr,20, 390);
     
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){
    
    bool bKey;
    bKey = myTracker.keyPressed(key);
	
    if(!bKey){
        float maxArea = 0;
        for (int i = 0; i < contourFinder.nBlobs; i++){
            if (contourFinder.blobs[i].area > maxArea){
                maxArea = contourFinder.blobs[i].area;
                currentPoint = contourFinder.blobs[i].centroid;
            }
        }
        
        switch (key){
            case 'q':
                sender.setup( HOST, PORT1 );
                break;
            case 'w':
                sender.setup( HOST, PORT2 );
                break;
            case 'e':
                sender.setup( HOST, PORT3 );
                break;
            case 'p':
                pixels --;
                if (pixels < 0) pixels = 0;
                break;
            case 'P':
                pixels ++;
                break;
            case 't':
                trig_thresh --;
                if (trig_thresh < 0) trig_thresh = 0;
                for(int i = 0; i < NUM_BEAMS; i++) myBeam[i]->setTimeThresh(trig_thresh);
                break;
            case 'T':
                trig_thresh ++;
                for(int i = 0; i < NUM_BEAMS; i++) myBeam[i]->setTimeThresh(trig_thresh);
                break;
            case 'd':
                dist_thresh -= 0.1;
                if (dist_thresh < 0) dist_thresh = 0;
                for(int i = 0; i < NUM_BEAMS; i++) myBeam[i]->setDistThresh(dist_thresh);
                break;
            case 'D':
                dist_thresh += 0.1;
                for(int i = 0; i < NUM_BEAMS; i++) myBeam[i]->setDistThresh(dist_thresh);
                break;
            // these are the keys assigned to setting config points
            // the lower config point of beam #1 is set by pressing '1'
            // the higher config point of beam #1 is set by pressing 'SHIFT + 1'
            // fill this in for the number of laser beams you are using (currently 3)
            case '1':
                if(maxArea){
                    myBeam[0]->setBeamLow(currentPoint);
                    updateXML(0, true);
                }
                break;
            case '!':
                if(maxArea){
                    myBeam[0]->setBeamHigh(currentPoint);
                    updateXML(0, false);
                }
                break;
            case '2':
                if(maxArea){
                    myBeam[1]->setBeamLow(currentPoint);
                    updateXML(1, true);
                }
                break;
            case '@':
                if(maxArea){
                    myBeam[1]->setBeamHigh(currentPoint);
                    updateXML(1, false);
                }
                break;
            case '3':
                if(maxArea){
                    myBeam[2]->setBeamLow(currentPoint);
                    updateXML(2, true);
                }
                break;
            case '#':
                if(maxArea){
                    myBeam[2]->setBeamHigh(currentPoint);
                    updateXML(2, false);
                }
                break;
        }
    }
}

//--------------------------------------------------------------
void testApp::updateXML(int beam, bool bLow){
    
    XML.pushTag("beam", beam);
    if(bLow){
        XML.setValue("x_l",currentPoint.x);
        XML.setValue("y_l",currentPoint.y);
    }else{
        XML.setValue("x_h",currentPoint.x);
        XML.setValue("y_h",currentPoint.y);
    }
    XML.popTag();
    XML.saveFile("config_points.xml");
}
