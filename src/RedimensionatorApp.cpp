/**
 *
 * Redimensionator (UNSTABLE ALPHA)
 * The Video Slit Scan Dimension Trader
 *
 * Copyleft 2011 
 * Forrest Oliphant, Sembiki Interactive, http://sembiki.com/
 * Cobbled together from Cinder examples and forum help. Thanks!
 * 
 * Made for Computational Photography class at Media Lab Helsinki, http://mlab.taik.fi/
 * 
 * Opens a video file, then trades the x-dimension for the time-dimension. Weird, word.
 * Output movies are split to match the dimensions of the input and named inputfilename.scan.0.mov etc.
 * What? Like this: http://www.youtube.com/view_play_list?p=B2540182DE868E85
 *
 **/

#include "cinder/app/AppBasic.h"
#include "cinder/Surface.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
//#include "cinder/Rand.h"
#include "cinder/qtime/QuickTime.h"
#include "cinder/ImageIo.h"
#include "cinder/Text.h"

#include "cinder/qtime/MovieWriter.h"
#include "cinder/ip/Fill.h"
//#include "Resources.h"

#include <iostream>

using namespace ci;
using namespace ci::app;
using std::string;

class Redimensionator : public AppBasic {
public:
  void prepareSettings( Settings *settings );
  void setup();
  
  void keyDown( KeyEvent event );
  void fileDrop( FileDropEvent event );
  
  void update();
  void draw();
  void shutdown();
  
  void loadMovieFile( const std::string &path );
  
  qtime::MovieSurface  inMovie;
  Surface              inSurface;
  Surface              outSurface;
  Surface              previewSurface;
  qtime::MovieWriter   outMovie;
  
  gl::Texture	textTexture;
  
  string inPath;
  int inW;
  int inH;
  int inFrames;
  int inFrame;
  
  int maxWidth;
  int outW;
  int outH;
  int data_size;
  int scanFrom;
  int scanTo;
  int scanIndex;
  int scanDiff;
  
  //GLubyte* data;
  Surface* frames;
  
  std::thread thread;
  void processFrames();
  
  int outCount;
  int* outWidths;
  int outIndex;
};

void Redimensionator::prepareSettings( Settings *settings )
{
  settings->setWindowSize( 1280, 720 );
  settings->setFullScreen( false );
  settings->setResizable( true );
}

void Redimensionator::setup()
{
  inFrame = 0;
  scanIndex = 0;
  outIndex = 0;
  
  // Setup inMovie
  inPath = getOpenFilePath();
  if( inPath.empty() )
    AppBasic::quit(); // User cancelled open
  
  loadMovieFile( inPath );
  
  // Get ready to scan
  if(inMovie)
  {
    inMovie.seekToStart();
    inMovie.play();
    inMovie.stop();
    inMovie.seekToStart();
    
    // Start scan thread
    std::thread thread(&Redimensionator::processFrames, this);
  } else {
    AppBasic::quit();
  }
}

void Redimensionator::keyDown( KeyEvent event )
{
  if( event.getChar() == 'q' ) {
    AppBasic::quit();
  }
}

void Redimensionator::loadMovieFile( const string &moviePath )
{
  try {
    inMovie = qtime::MovieSurface( moviePath );
    
    console() << "Dimensions:" << inMovie.getWidth() << " x " << inMovie.getHeight() << std::endl;
    console() << "Duration:  " << inMovie.getDuration() << " seconds" << std::endl;
    console() << "Frames:    " << inMovie.getNumFrames() << std::endl;
    console() << "Framerate: " << inMovie.getFramerate() << std::endl;
    console() << "Alpha channel: " << inMovie.hasAlpha() << std::endl;		
    console() << "Has audio: " << inMovie.hasAudio() << " Has visuals: " << inMovie.hasVisuals() << std::endl;
    
    inW = inMovie.getWidth();
    inH = inMovie.getHeight();
    inFrames = inMovie.getNumFrames();
    
    // Seems to crash when the array reaches a billion pixels on my MBP with 4GB RAM
    maxWidth = 1920;
    while ((inW * inH * maxWidth) >= 500000000)
      maxWidth -= 20;
    
    // Figure out how many video files to make
    outCount = inFrames/maxWidth;
    // Remainder
    if (inFrames%maxWidth > 0)
      outCount++;
    
    outWidths = new int[outCount];
    for (int i=0; i < outCount; ++i) {
      outWidths[i] = maxWidth;
    }
    if (inFrames%maxWidth > 0)
      outWidths[outCount-1] = inFrames%maxWidth;
  }
  catch( ... ) {
    console() << "Unable to load the movie." << std::endl;
  }	
}

void Redimensionator::fileDrop( FileDropEvent event )
{
  loadMovieFile( event.getFile( 0 ) );
}

void Redimensionator::update()
{
  if( inMovie && outMovie)
  {
    // Info
    std::stringstream info;
    info << "inFrame " << inFrame+1 << " / " << outW << std::endl;
    std::stringstream info2;
    info2 << "outFrame " << scanIndex+1 << " / " << scanTo << std::endl;
    std::stringstream info3;
    info3 << "outMovie " << outIndex+1 << " / " << outCount << std::endl;
    
    TextLayout layout;
    layout.setColor( Color( 1, 1, 1 ) );
    layout.setFont( Font( "Verdana", 20 ) );
    layout.addLine( info.str() );
    layout.addLine( info2.str() );
    layout.addLine( info3.str() );
    Surface rendered = layout.render( true, false );
    textTexture = gl::Texture( rendered );
  }
}

void Redimensionator::draw()
{
  gl::clear( Color( 0, 0, 0 ) );
  
  // Preview
  if( previewSurface )
    gl::draw( gl::Texture( previewSurface ) ); 
  
  // Info
  if( textTexture )
    gl::draw( textTexture, Vec2f( 10, 10 ) );
  
}

void Redimensionator::processFrames()
{
  outIndex = 0;
  while (outIndex < outCount) {
    
    outW = outWidths[outIndex];
    outH = inH;
    scanFrom = 0;
    scanTo = inW;
    scanDiff = scanTo-scanFrom;
    
    // Setup outMovie
    std::stringstream outPath; 
    outPath << inPath << ".scan." << outIndex << ".mov";
    try {
      outMovie = qtime::MovieWriter( outPath.str(), outW, outH, qtime::MovieWriter::Format::Format() );
    }
    catch( ... ) {
      console() << "Unable to save the movie." << std::endl;
    }	
    
    // Clear
    //Surface outSurface( outW, outH, false );
    previewSurface = Surface(inW, inH, SurfaceChannelOrder::RGB);
    
    // Save all frames to memory
    frames = new Surface[outW];
    for (int i = 0; i < outW; ++i) {
      inFrame = i;
      try {
        inMovie.seekToFrame(outIndex*maxWidth+i);
        inSurface = inMovie.getSurface();
        frames[i] = inSurface.clone();
      }
      catch( ... ) {
        console() << "Probably out of memory." << std::endl;
      } 
      previewSurface = inSurface.clone();
    }
    
    outSurface = Surface(outW, outH, SurfaceChannelOrder::RGB);
    previewSurface = Surface(outW, outH, SurfaceChannelOrder::RGB);
    
    scanIndex = scanFrom;
    while (scanIndex < scanTo)
    {
      // Slitscan
      for (int x = 0; x < outW; ++x) {
        inFrame = x;
        
        inSurface = frames[x];
        
        outSurface.copyFrom(inSurface, Area( scanIndex, 0, scanIndex+1, outH ), Vec2i( x-scanIndex, 0 ) );
      }
      
      // Write frame to new mov
      outMovie.addFrame( outSurface );
      previewSurface = outSurface.clone();
      
      scanIndex++;
    }
    
    // Done with these frames, free memory
    delete [] frames;
    frames = NULL;
    
    outIndex++;
  }
  
  AppBasic::quit();
}

void Redimensionator::shutdown() {
  delete [] frames;
}

CINDER_APP_BASIC( Redimensionator, RendererGl )