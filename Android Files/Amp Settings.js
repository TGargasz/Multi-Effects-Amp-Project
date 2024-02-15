//  ***************************************
//  * Amp Settings                        *
//  * R0 - Initial development            *
//  * R1 - Initial release                *
//  * R2 - Add Flanger and Reverb         *
//  * R3 - Add Footswitches tab           *
//  * R4 - Add Front Panel tab            *
//  ***************************************

// Global vars
var ip, port, net;
var startUp = true;
var mode = 0;
var swtch = [ ];
var rate = 5;
var depth = 50;
var delay = 500;
var feedback = 50;
var gain = 5;
var mix = 50;
var hifreq = 18000.00;
var amount = 0.50;
var speed = 0.20;
var range = 0.75;
var color = 0.75;
var flngFilter = 0;
var rateKnob = 1;
var depthKnob = 2;
var txtSize = 24;
var txtSize2 = 20;
var txtSize3 = 18;

// Math rounding functions
( function() 
{
    /*
    * Decimal adjustment of a number.
    *
    * @param {String}  type  The type of adjustment.
    * @param {Number}  value The number.
    * @param {Integer} exp   The exponent (the 10 logarithm of the adjustment base).
    * @returns {Number} The adjusted value.
    */
    function decimalAdjust(type, value, exp) 
    {
        // If the exp is undefined or zero...
        if (typeof exp === 'undefined' || +exp === 0) 
        {
            return Math[type](value);
        }
        value = +value;
        exp = +exp;
        // If the value is not a number or the exp is not an integer...
        if (isNaN(value) || !(typeof exp === 'number' && exp % 1 === 0)) 
        {
             return NaN;
        }
        // Shift
        value = value.toString().split('e');
        value = Math[type](+(value[0] + 'e' + (value[1] ? (+value[1] - exp) : -exp)));
        // Shift back
        value = value.toString().split('e');
        return +(value[0] + 'e' + (value[1] ? (+value[1] + exp) : exp));
    }
    
    // Decimal round
    if (!Math.round10) 
    {
        Math.round10 = function(value, exp) 
        {
            return decimalAdjust('round', value, exp);
        };
    }
    
    // Decimal floor
    if (!Math.floor10) 
    {
        Math.floor10 = function(value, exp) 
        {
            return decimalAdjust('floor', value, exp);
        };
    }
    
    // Decimal ceil
    if (!Math.ceil10) 
    {
        Math.ceil10 = function(value, exp) 
        {
            return decimalAdjust('ceil', value, exp);
        };
    }
}
)();

// Called when application is started
function OnStart()
{
    // *** ESP8266 connection details ***
    ssid = "GCA-Amp";
    pwrd = "123456789";
    ip   = "192.168.4.1";
    port = 80;
    
    // Debug client connection details
    // ip = "192.168.128.248";
    
    //Create the main graphical layout.
	CreateLayout();	

    // Check wifi is enabled.
    thisIp = app.GetIPAddress();
    if( thisIp == "0.0.0.0" ) 
    {
        app.Alert( "Please Enable Wi-Fi!","Wi-Fi Not Detected","OK" );
        setTimeout(app.Exit,5000);
    }

    app.PreventWifiSleep();
    app.PreventScreenLock( true );
    app.EnableBackKey(false);
    app.SetOrientation( "Portrait" );
    
	//Create network objects for communication with ESP8266.
	net = app.CreateNetClient( "TCP,Raw" );  
	net.SetOnConnect( net_OnConnect );

	// Will refresh settings on start only
    net.Connect( ip, port );
} // ~~~ End of OnStart() ~~~

// Create the graphical layout
function CreateLayout()
{
	// Create a layout with objects vertically centered
    lay = app.CreateLayout( "linear", "VCenter,FillXY" );
    lay.SetBackColor("black");
    //lay.SetBackground( "/Sys/Img/BlueBack.jpg" );
    
    // Create a text label and add it to layout
	txt = app.CreateText( "Gargasz Custom Audio" );
	txt.SetBackColor( "black" );
	txt.SetTextColor( "white" );
	txt.SetTextSize( 28 );
	txt.SetMargins( 0, 0, 0, 0.02 );
	lay.AddChild( txt );
	
    // Create tabs
    var tabs = app.CreateTabs( "EFFECTS,FOOTSWITCHES,KNOBS", 1.0, 0.82, "VCenter,FillXY" );
    layEffects = tabs.GetLayout( "EFFECTS" );
    layEffects.SetBackColor("black");
    laySwitches = tabs.GetLayout( "FOOTSWITCHES" );
    laySwitches.SetBackColor("black");
    layFrntPnl = tabs.GetLayout( "KNOBS" );
    layFrntPnl.SetBackColor("black");
    lay.AddChild( tabs );
    
    // Create ~~~~~~~~ Send toggle button ~~~~~~~~
	btnSend = app.CreateToggle( "  Send to Amplifier  " );
	btnSend.SetOnTouch( btnSend_OnTouch );
	btnSend.SetMargins( 0, 0.01, 0, 0.01 );
	btnSend.SetTextColor("white");
	lay.AddChild( btnSend );
	
    // Create screen scrollers
    scroll1 = app.CreateScroller( 1.0, 0.8 );
    scroll1.SetBackColor("black");
    scroll2 = app.CreateScroller( 1.0, 0.8 );
    scroll2.SetBackColor("black");
    scroll3 = app.CreateScroller( 1.0, 0.8 );
    scroll3.SetBackColor("black");
    layEffects.AddChild( scroll1 );
    laySwitches.AddChild( scroll2 );
    layFrntPnl.AddChild( scroll3 );
    
    // Create layouts inside scrollers
    layScroll = app.CreateLayout( "linear", "VCenter,FillXY" );
    layScrol2 = app.CreateLayout( "linear", "VCenter,FillXY" );
    layScrol3 = app.CreateLayout( "linear", "VCenter,FillXY" );
    scroll1.AddChild( layScroll );
    scroll2.AddChild( layScrol2 );
    scroll3.AddChild( layScrol3 );
    
    //~~~~~~~~~~~~~~~~~~~~ Effects Layout ~~~~~~~~~~~~~~~~~~~~~~
    
	// Tremolo check box
	txtTrem = app.CreateText( "Tremolo", 1.0, 0.04, "FontAwesome,Html" );
	txtTrem.SetMargins( 0, 0.06, 0, 0 );
	txtTrem.SetTextSize( txtSize );
	layScroll.AddChild( txtTrem );
	
	// Create Tremolo Rate text label
	txtRate = app.CreateText( "Rate: 5 cps" );
	txtRate.SetMargins( 0, 0.01, 0, 0 );
	layScroll.AddChild( txtRate );
	
	// Create Tremolo Rate seek bar 
	skbRate = app.CreateSeekBar( 0.9 );
	skbRate.SetRange( 10 );
	skbRate.SetValue( 5 );
	skbRate.SetOnTouch( skbRate_OnTouch );
	layScroll.AddChild( skbRate );
	
	// Create Tremolo Depth text label
	txtDepth = app.CreateText( "Depth: 50 %" );
	txtDepth.SetMargins( 0, 0.01, 0, 0 );
	layScroll.AddChild( txtDepth );
	
	// Create Tremolo Depth seek bar
	skbDepth = app.CreateSeekBar( 0.9, -1 );
	skbDepth.SetRange( 100 );
	skbDepth.SetValue( 50 );
	skbDepth.SetOnTouch( skbDepth_OnTouch );
	layScroll.AddChild( skbDepth );
	
	// Distortion check box
	txtDist = app.CreateText( "Distortion", 1.0, 0.04, "FontAwesome,Html" );
	txtDist.SetMargins( 0, 0.03 );
	txtDist.SetTextSize( txtSize );
	layScroll.AddChild( txtDist );
	
	// Create Distortion Gain text label
	txtGain = app.CreateText( "Drive: 50 %" );
	txtGain.SetMargins( 0, 0.01, 0, 0 );
	layScroll.AddChild( txtGain );
	
	// Create Distortion Gain seek bar 
	skbGain = app.CreateSeekBar( 0.9, -1 );
	skbGain.SetRange( 100 );
	skbGain.SetValue( 50 );
	skbGain.SetOnTouch( skbGain_OnTouch );
	layScroll.AddChild( skbGain );
	
	// Create Distortion Mix text label 
	txtMix = app.CreateText( "Mix: 50 %" );
	txtMix.SetMargins( 0, 0.01, 0, 0 );
	layScroll.AddChild( txtMix );
	
	// Create Distortion Mix seek bar 
	skbMix = app.CreateSeekBar( 0.9, -1 );
	skbMix.SetRange( 100 );
	skbMix.SetValue( 50 );
	skbMix.SetOnTouch( skbMix_OnTouch );
	layScroll.AddChild( skbMix );
	
	// Flange check box
	txtFlange = app.CreateText( "Flanger", 1.0, 0.04, "FontAwesome,Html" );
	txtFlange.SetMargins( 0, 0.03 );
	txtFlange.SetTextSize( txtSize );
	layScroll.AddChild( txtFlange );
	
	// Create Flange Speed text label 
	txtSpeed = app.CreateText( "Rate: 0.20 cps" );
	txtSpeed.SetMargins( 0, 0.01, 0, 0 );
	layScroll.AddChild( txtSpeed );
	
	// Create Flange Delay seek bar 
	skbSpeed = app.CreateSeekBar( 0.9, -1 );
	skbSpeed.SetRange( 2 );
	skbSpeed.SetValue( 0.20 );
	skbSpeed.SetOnTouch( skbSpeed_OnTouch );
	layScroll.AddChild( skbSpeed );
	
	// Create Flange Range text label 
	txtRange = app.CreateText( "Range: 75 %" );
	txtRange.SetMargins( 0, 0.01, 0, 0 );
	layScroll.AddChild( txtRange );
	
	// Create Flange Feedback seek bar 
	skbRange = app.CreateSeekBar( 0.9, -1 );
	skbRange.SetRange( 100 );
	skbRange.SetValue( 75 );
	skbRange.SetOnTouch( skbRange_OnTouch );
	layScroll.AddChild( skbRange );
	
	// Create Flange 'Color' text label 
	txtColor = app.CreateText( "Color: 50 %" );
	txtColor.SetMargins( 0, 0.01, 0, 0 );
	layScroll.AddChild( txtColor );
	
	// Create Flange 'Color' seek bar 
	skbColor = app.CreateSeekBar( 0.9, -1 );
	skbColor.SetRange( 100 );
	skbColor.SetValue( 50 );
	skbColor.SetOnTouch( skbColor_OnTouch );
	layScroll.AddChild( skbColor );
	
	// Filter Mode check box
	var text6 = "<font color=#aa0000>[fa-square-o]</font> Flanger Filter Mode" ;
	txtFiltMode = app.CreateText( text6, 1.0, 0.04, "FontAwesome,Html" );
	txtFiltMode.SetMargins( 0, 0.02 );
	txtFiltMode.SetOnTouchUp( OnTouchFiltMode );
	txtFiltMode.SetTextSize( txtSize2 );
    txtFiltMode.isChecked = false; 
	layScroll.AddChild( txtFiltMode );
		
	// Echo check box
    txtEcho = app.CreateText( "Echo", 1.0, 0.04, "FontAwesome,Html" );
	txtEcho.SetMargins( 0, 0.03 );
	txtEcho.SetTextSize( txtSize );
	layScroll.AddChild( txtEcho );
	
	// Create Echo Delay text label
	txtDelay = app.CreateText( "Delay: 500 ms" );
	txtDelay.SetMargins( 0, 0.01, 0, 0 );
	layScroll.AddChild( txtDelay );
	
	// Create Echo Delay seek bar
	skbDelay = app.CreateSeekBar( 0.9, -1 );
	skbDelay.SetRange( 1000 );
	skbDelay.SetValue( 500 );
	skbDelay.SetOnTouch( skbDelay_OnTouch );
	layScroll.AddChild( skbDelay );
	
	// Create Echo Feedback text label
	txtFb = app.CreateText( "Feedback: 50 %" );
	txtFb.SetMargins( 0, 0.01, 0, 0 );
	layScroll.AddChild( txtFb );
	
	// Create Echo Feedback seek bar
	skbFb = app.CreateSeekBar( 0.9, -1 );
	skbFb.SetRange( 95 );
	skbFb.SetValue( 50 );
	skbFb.SetOnTouch( skbFb_OnTouch );
	layScroll.AddChild( skbFb );
	
	// Reverb check box
	txtReverb = app.CreateText( "Reverb", 1.0, 0.04, "FontAwesome,Html" );
	txtReverb.SetMargins( 0, 0.03 );
	txtReverb.SetTextSize( txtSize );
	layScroll.AddChild( txtReverb );
	
	// Create Reverb Hi Cut Off text label 
	txtHiFreq = app.CreateText( "Shimmer: 9000 Hz" );
	txtHiFreq.SetMargins( 0, 0.01, 0, 0 );
	layScroll.AddChild( txtHiFreq  );
	
	// Create Reverb Rate seek bar 
	skbHiFreq = app.CreateSeekBar( 0.9, -1 );
	skbHiFreq.SetRange( 20000 );
	skbHiFreq.SetValue( 9000 );
	skbHiFreq.SetOnTouch( skbHiFreq_OnTouch );
	layScroll.AddChild( skbHiFreq );
	
	// Create Reverb Amount text label 
	txtAmount = app.CreateText( "Depth: 50 %" );
	txtAmount.SetMargins( 0, 0.01, 0, 0 );
	layScroll.AddChild( txtAmount );
	
	// Create Reverb Amount seek bar 
	skbAmount = app.CreateSeekBar( 0.9, -1 );
	skbAmount.SetMargins( 0, 0.01, 0, 0.13 );
	skbAmount.SetRange( 100 );
	skbAmount.SetValue( 50 );
	skbAmount.SetOnTouch( skbAmount_OnTouch );
	layScroll.AddChild( skbAmount );
	
	// Initially scroll to center of image 
    scroll1.ScrollTo( 0.5, 0.5 );
    
    //~~~~~~~~~~~~~~~~~~~~ Foot Switch Layout ~~~~~~~~~~~~~~~~~~~~~~
    
    // Build arrays of controls
    txtTempSw = [];
    txtTremSw = [];
    txtEchoSw = [];
    txtDistSw = [];
    txtRevbSw = [];
    txtFlngSw = []; 
    for(i=0;i<3;i++)
    {
        txtTempSw[i] = ".  <font color=#aa0000>[fa-square-o]</font> Tremolo";
        txtTremSw[i]=app.CreateText(txtTempSw[i], 1.0, 0.04, "FontAwesome,Html,Left");
    	txtTremSw[i].SetOnTouchUp( OnTouchTremSw );
    	txtTremSw[i].SetTextSize( txtSize2 );
        txtTremSw[i].isChecked = false;
        
        txtTempSw[i] = ".  <font color=#aa0000>[fa-square-o]</font> Distortion";
        txtDistSw[i]=app.CreateText(txtTempSw[i], 1.0, 0.04, "FontAwesome,Html,Left");
    	txtDistSw[i].SetOnTouchUp( OnTouchDistSw );
    	txtDistSw[i].SetTextSize( txtSize2 );
        txtDistSw[i].isChecked = false;
        
        txtTempSw[i] = ".  <font color=#aa0000>[fa-square-o]</font> Flanger";
        txtFlngSw[i]=app.CreateText(txtTempSw[i], 1.0, 0.04, "FontAwesome,Html,Left");
    	txtFlngSw[i].SetOnTouchUp( OnTouchFlngSw );
    	txtFlngSw[i].SetTextSize( txtSize2 );
        txtFlngSw[i].isChecked = false;
        
        txtTempSw[i] = ".  <font color=#aa0000>[fa-square-o]</font> Echo";
        txtEchoSw[i]=app.CreateText(txtTempSw[i], 1.0, 0.04, "FontAwesome,Html,Left");
    	txtEchoSw[i].SetOnTouchUp( OnTouchEchoSw );
    	txtEchoSw[i].SetTextSize( txtSize2 );
        txtEchoSw[i].isChecked = false;
        
        txtTempSw[i] = ".  <font color=#aa0000>[fa-square-o]</font> Reverb";
        txtRevbSw[i]=app.CreateText(txtTempSw[i], 1.0, 0.04, "FontAwesome,Html,Left");
    	txtRevbSw[i].SetOnTouchUp( OnTouchRevbSw );
    	txtRevbSw[i].SetTextSize( txtSize2 );
        txtRevbSw[i].isChecked = false;
    }
    
    // Left Footswitch title
	txtLeftSwitch = app.CreateText( "Left Footswitch", 1.0, 0.04 );
	txtLeftSwitch.SetMargins( 0, 0.07, 0, 0.01 );
	txtLeftSwitch.SetTextSize( txtSize );
    layScrol2.AddChild( txtLeftSwitch );
    
    // Check boxes
	layScrol2.AddChild(txtTremSw[0]);
	layScrol2.AddChild(txtDistSw[0]);
	layScrol2.AddChild(txtFlngSw[0]);
	layScrol2.AddChild(txtEchoSw[0]);
	layScrol2.AddChild(txtRevbSw[0]);
	
    // Middle Footswitch title
	txtMidSwitch = app.CreateText( "Middle Footswitch", 1.0, 0.04 );
	txtMidSwitch.SetMargins( 0, 0.03, 0, 0.01 );
	txtMidSwitch.SetTextSize( txtSize );
    layScrol2.AddChild( txtMidSwitch );
    
    // Check boxes
	layScrol2.AddChild(txtTremSw[1]);
	layScrol2.AddChild(txtDistSw[1]);
	layScrol2.AddChild(txtFlngSw[1]);
	layScrol2.AddChild(txtEchoSw[1]);
	layScrol2.AddChild(txtRevbSw[1]);
	
    // Right Footswitch title
	txtRightSwitch = app.CreateText( "Right Footswitch", 1.0, 0.05 );
	txtRightSwitch.SetMargins( 0, 0.03, 0, 0.01 );
	txtRightSwitch.SetTextSize( txtSize );
    layScrol2.AddChild( txtRightSwitch );
	
	// Check boxes
	layScrol2.AddChild(txtTremSw[2]);
	layScrol2.AddChild(txtDistSw[2]);
	layScrol2.AddChild(txtFlngSw[2]);
	layScrol2.AddChild(txtEchoSw[2]);
	layScrol2.AddChild(txtRevbSw[2]);
	
	// Add some margin to the bottom
	txtRevbSw[2].SetMargins(  0, 0, 0, 0.055 );
	
    //Initially scroll to center of image 
    scroll2.ScrollTo( 0.5, 0.5 );
    
    //~~~~~~~~~~~~~~~~~~~~ Front Panel Layout ~~~~~~~~~~~~~~~~~~~~~~
    
    // Knob Assignment Selection List
    txtTempSw[3] = "NA: (Not Assigned),Tremolo: Rate,Tremolo: Depth," +
    "Distortion: Gain,Distortion: Depth," +
    "Flanger: Rate,Flanger: Range,Flanger: Color," +
    "Echo: Delay,Echo: Feedback," +
    "Reverb: Shimmer,Reverb: Depth";
    
    // Rate Knob Title
	txtRateKnob = app.CreateText( "Rate Knob", 1.0, 0.05 );
	txtRateKnob.SetMargins( 0, 0.07, 0, 0.01 );
	txtRateKnob.SetTextSize( txtSize );
    layScrol3.AddChild( txtRateKnob );
    
    // Rate Assignment Knob Selector
    spinRate = app.CreateSpinner(txtTempSw[3], 0.8 );
    spinRate.SetOnChange( OnRateSelect );
    spinRate.SetTextColor( "#aaaaaa" );
    spinRate.SetTextSize( txtSize2 );
    layScrol3.AddChild( spinRate );
    
    // Depth Knob Title
    txtDepthKnob = app.CreateText( "Depth Knob", 1.0, 0.05 );
	txtDepthKnob.SetMargins( 0, 0.03, 0, 0.01 );
	txtDepthKnob.SetTextSize( txtSize );
    layScrol3.AddChild( txtDepthKnob );
    
    // Depth  Assignment Knob Selector
    spinDepth = app.CreateSpinner(txtTempSw[3], 0.8 );
    spinDepth.SetOnChange( OnDepthSelect );
    spinDepth.SetTextColor( "#aaaaaa" );
    spinDepth.SetTextSize( txtSize2 );
    layScrol3.AddChild( spinDepth );
    
	//Add layout to app     
    app.AddLayout( lay );
} // ~~~ End of CreateLayout() ~~~
	
// Touch Flanger Filter check 
function OnTouchFiltMode( ev )
{
    btnSend.SetTextColor("yellow");
    if( this.isChecked ) 
    {
        this.SetHtml( "<font color=#aa0000>[fa-square-o]</font> Flanger Filter Mode" );
        flngFilter = 0;
    }
    else
    {
        this.SetHtml( "<font color=#008800>[fa-check-square-o]</font> Flanger Filter Mode" );
        flngFilter = 1;
    }   
    this.isChecked = !this.isChecked;
} 

// Touch Foot Switch checkbox
function OnTouchTremSw( ev )
{
    btnSend.SetTextColor("yellow");
    if( this.isChecked )
    {
        this.SetHtml( ".  <font color=#aa0000>[fa-square-o]</font> Tremolo" );
    }
    else
    {
        this.SetHtml( ".  <font color=#008800>[fa-check-square-o]</font> Tremolo" );
    }    
    this.isChecked = !this.isChecked;
    calcAssigns();
}
function OnTouchEchoSw( ev )
{
    btnSend.SetTextColor("yellow");
    if( this.isChecked ) 
    {
        this.SetHtml( ".  <font color=#aa0000>[fa-square-o]</font> Echo" );
    }
    else
    {
        this.SetHtml( ".  <font color=#008800>[fa-check-square-o]</font> Echo" );
    }   
    this.isChecked = !this.isChecked;
    calcAssigns();
}
function OnTouchDistSw( ev )
{
    btnSend.SetTextColor("yellow");
    if( this.isChecked )
    {
        this.SetHtml( ".  <font color=#aa0000>[fa-square-o]</font> Distortion" );
    }
    else
    {
        this.SetHtml( ".  <font color=#008800>[fa-check-square-o]</font> Distortion" );
    }   
    this.isChecked = !this.isChecked;
    calcAssigns();
}
function OnTouchRevbSw( ev )
{
    btnSend.SetTextColor("yellow");
    if( this.isChecked )
    {
        this.SetHtml( ".  <font color=#aa0000>[fa-square-o]</font> Reverb" );
    }
    else
    {
        this.SetHtml( ".  <font color=#008800>[fa-check-square-o]</font> Reverb" );
    }    
    this.isChecked = !this.isChecked;
    calcAssigns();
}
function OnTouchFlngSw( ev )
{
    btnSend.SetTextColor("yellow");
    if( this.isChecked ) 
    {
        this.SetHtml( ".  <font color=#aa0000>[fa-square-o]</font> Flanger" );
    }
    else
    {
        this.SetHtml( ".  <font color=#008800>[fa-check-square-o]</font> Flanger" );
    }   
    this.isChecked = !this.isChecked;
    calcAssigns();
}

// Touch a Setting (seek bar)
function skbRate_OnTouch( value )
{
	txtRate.SetText( "Rate: " + value + " cps" );
	rate = value;
	btnSend.SetTextColor("yellow");
}
function skbDepth_OnTouch( value )
{
	txtDepth.SetText( "Depth: " + value + " %" );
	depth = value;
	btnSend.SetTextColor("yellow");
}
function skbDelay_OnTouch( value )
{
	if( value <= 1 ) 
	{
	    value = 1;
	    skbDelay.SetValue( 1 );
	}
	txtDelay.SetText( "Delay: " + value + " ms" );
	delay = value;
	btnSend.SetTextColor("yellow");
}
function skbFb_OnTouch( value )
{
	txtFb.SetText( "Feedback: " + value + " %" );
	feedback = value;
	btnSend.SetTextColor("yellow");
}
function skbGain_OnTouch( value )
{
	txtGain.SetText( "Drive: " + value + " %"  );
	gain = value;
	btnSend.SetTextColor("yellow");
}
function skbMix_OnTouch( value )
{
	txtMix.SetText( "Mix: " + value + " %" );
	mix = value;
	btnSend.SetTextColor("yellow");
}
function skbHiFreq_OnTouch( value )
{
	txtHiFreq.SetText( "Shimmer: " + value + " Hz" );
	hifreq = value;
	btnSend.SetTextColor("yellow");
}
function skbAmount_OnTouch( value )
{
	txtAmount.SetText( "Depth: " + value + " %" );
	amount = value;
	btnSend.SetTextColor("yellow");
}
function skbSpeed_OnTouch( value )
{
	txtSpeed.SetText( "Rate: " + value + " cps" );
	speed = value;
	btnSend.SetTextColor("yellow");
}
function skbRange_OnTouch( value )
{
	txtRange.SetText( "Range: " + value + " %" );
	range = value;
	btnSend.SetTextColor("yellow");
}
function skbColor_OnTouch( value )
{
	txtColor.SetText( "Color: " + value + " %" );
	color = value;
	btnSend.SetTextColor("yellow");
}

// Called when user selects a Front Panel (Knob) Assignment
function OnRateSelect( item, index )
{
    btnSend.SetTextColor("yellow");
    rateKnob = index;
}
function OnDepthSelect( item, index )
{
    btnSend.SetTextColor("yellow");
    depthKnob = index;
}

// [Send to Amp] button
function btnSend_OnTouch()
{
    btnSend.SetTextColor("white");
    
    // Check wifi is present.
    thisIp = app.GetIPAddress();
    if( thisIp == "0.0.0.0" ) 
    {
        app.Alert( "Please Enable Wi-Fi!","Wi-Fi Not Detected","OK" );
        setTimeout(app.Exit,5000);
    }
    
	if( !net.IsConnected() ) 
	{
		// Connect to ESP
		net.Connect( ip, port );
	}
	else 
	{ 
		// Disconnect from ESP
		btnSend.SetChecked( false );
		net.Disconnect();
		net = "";
		net = app.CreateNetClient( "TCP,Raw" );
	    net.SetOnConnect( net_OnConnect );
	}
}

// Called by [Send to Amp] button when WiFi connects to ESP
function net_OnConnect( connected )
{
    var req = "";
    var rate_adj = 0;
    var depth_adj = 0;
    var delay_adj = 1;
    var feedback_adj = 0;
    var gain_adj = 0.50;
    var mix_adj = 0.50;
    var hifreq_adj = 18000.00;
	var amount_adj = 0.50;
	var speed_adj = 0.15;
	var range_adj = 0.75;
	var color_adj = 0.60;
    
	if( connected ) 
	{
	    btnSend.SetChecked( true );
	    
	    if( startUp == true)
	    {
    	    req = "GET /putsets HTTP/1.1\r\n";
    	    btnSend.SetTextColor( "white" );
    	    startUp = false;
	    }
	    else
	    {
	        rateKnob_num = rateKnob;
	        // Convert SeekBar Values to Amp Settings
	        delay_adj = delay * 48;
            rate_adj = rate;
            gain_adj = gain / 100;
            feedback_adj = feedback / 100;
            depth_adj = depth / 100;
            mix_adj = mix / 100;
            hifreq_adj = hifreq;
        	amount_adj = amount / 100;
        	speed_adj = speed;
        	range_adj = range / 100;
        	color_adj = color / 100;

	        // Create HTTP request
            req = "GET /getsets/ " + swtch[0] + " " + swtch[1] + " "  + 
            swtch[2] + " " + rate_adj + " " + depth_adj + " " + delay_adj + " " + 
            feedback_adj + " " + gain_adj + " " + mix_adj + " " + hifreq_adj + " " + 
            amount_adj + " " + speed_adj + " " + range_adj + " " + color_adj + " " + 
            flngFilter + " " + rateKnob + " " + depthKnob + " HTTP/1.1\r\n";
	    }

		//Send request to ESP8266
	    net.SendText( req, "UTF-8" );
	    
	    var msg = "", s = "";
        do msg += s = net.ReceiveText( "UTF-8" );
        while( s.length > 0 );
        
        var items = msg.split(' ');
        var itemCount = items.length;
        
        if ( itemCount == 21 )
        {
            // Parse the settings
            swtch[0] = items[4];
            swtch[1] = items[5];
            swtch[2] = items[6];
            delay_adj = items[7];
            rate_adj = items[8];
            gain_adj = items[9];
            feedback_adj = items[10];
            depth_adj = items[11];
            mix_adj = items[12];
            hifreq_adj = items[13];
        	amount_adj = items[14];
        	speed_adj = items[15];
        	range_adj = items[16];
        	color_adj = items[17];
        	flngFilter = items[18];
        	rateKnob = items[19];
        	depthKnob = items[20];
	        
            // Convert amp settings to SeekBar Values
            delay = delay_adj / 48;
            delay = Math.round10( delay, -2);
            
            rate = rate_adj;
            rate = Math.round10( rate, -2);
            
            gain = gain_adj * 100;
            gain = Math.round10( gain, -2);
            
            feedback = feedback_adj * 100;
            feedback = Math.round10( feedback, -2 );
            
            depth = depth_adj * 100;
            depth = Math.round10( depth, -2 );
            
            mix = mix_adj * 100;
            mix = Math.round10( mix, -2 );
            
            hifreq = hifreq_adj;
            hifreq = Math.round10( hifreq, 0 );
            
            amount = amount_adj * 100;
            amount = Math.round10( amount, -2 );
            
            speed = speed_adj;
            speed = Math.round10( speed, -2);
            
            range = range_adj * 100;
            range = Math.round10( range, -2 );
            
            color = color_adj * 100;
            color = Math.round10( color, -2 );
            
            // Set the controls
            setAssigns();
            setKnobAssigns();
            
            // Tremolo settings
            skbRate.SetValue( rate );
            skbDepth.SetValue( depth );
            
            // Echo settings
            skbDelay.SetValue( delay );
            skbFb.SetValue( feedback );
            
            // Distort settings
            skbGain.SetValue( gain );
            skbMix.SetValue( mix );
            
            // Reverb settings
            skbHiFreq.SetValue( hifreq );
            skbAmount.SetValue( amount );
            
            // Flanger settings
            skbSpeed.SetValue( speed );
            skbRange.SetValue( range );
            skbColor.SetValue( color );
            
            // Tremolo labels
            txtRate.SetText( "Rate: " + rate + " cps" );
            txtDepth.SetText( "Depth: " + depth + " %" );
            
            // Echo labels
            txtDelay.SetText( "Delay: " + delay + " ms" );
            txtFb.SetText( "Feedback: " + feedback + " %" );
            
            // Distort labels
            txtGain.SetText( "Drive: " + gain + " %" );
            txtMix.SetText( "Mix: " + mix + " %" );
            
            // Reverb labels
            txtHiFreq.SetText( "Shimmer: " + hifreq + " Hz" );
            txtAmount.SetText( "Depth: " + amount + " %" );
            
            // Flanger labels
            txtSpeed.SetText( "Rate: " + speed + " cps" );
            txtRange.SetText( "Range: " + range + " %" );
            txtColor.SetText( "Color: " + color + " %" );
            
            app.ShowPopup( "Status OK", "Bottom,Short" );
        }
        else
        {
            app.ShowPopup( "ERROR reading from amp", "Bottom,Short" );
        }
      
        net.Disconnect();
        net = "";
		net = app.CreateNetClient( "TCP,Raw" );
	    net.SetOnConnect( net_OnConnect );
        btnSend.SetChecked( false );
	}
}

// Calculate Footswitch checkbox values
function calcAssigns()
{
    for(i=0;i<3;i++)
    {
        swtch[i] = 0;
        
        if ( txtTremSw[i].isChecked )
        {
            swtch[i] += 1;
        }
        
        if ( txtDistSw[i].isChecked )
        {
            swtch[i] += 2;
        }
        
        if ( txtFlngSw[i].isChecked )
        {
            swtch[i] += 4;
        }
        
        if ( txtEchoSw[i].isChecked )
        {
            swtch[i] += 8;
        }
        
        if ( txtRevbSw[i].isChecked )
        {
            swtch[i] += 16;
        }
        
    }
}

// Set Footswitch checkboxes
function setAssigns()
{
    /* Footswitch assignment bit order
        Bit Val Assignment
        ------------------
        0   1   Tremolo
        1   2   Distortion
        2   4   Flanger
        3   8   Echo 
        4   16  Reverb
    */
    for(i=0;i<3;i++)
    {
        txtTremSw[i].isChecked = false;
        txtTremSw[i].SetHtml( ".  <font color=#aa0000>[fa-square-o]</font> Tremolo" );
        txtDistSw[i].isChecked = false;
        txtDistSw[i].SetHtml( ".  <font color=#aa0000>[fa-square-o]</font> Distortion" );
        txtFlngSw[i].isChecked = false;
        txtFlngSw[i].SetHtml( ".  <font color=#aa0000>[fa-square-o]</font> Flanger" );
        txtEchoSw[i].isChecked = false;
        txtEchoSw[i].SetHtml( ".  <font color=#aa0000>[fa-square-o]</font> Echo" );
        txtRevbSw[i].isChecked = false;
        txtRevbSw[i].SetHtml( ".  <font color=#aa0000>[fa-square-o]</font> Reverb" );
        
        if ((swtch[i] & 1))
        {
            txtTremSw[i].isChecked = true;
            txtTremSw[i].SetHtml( ".  <font color=#008800>[fa-check-square-o]</font> Tremolo" );  
        }
        
        if ((swtch[i] & 2))
        {
            txtDistSw[i].isChecked = true;
            txtDistSw[i].SetHtml( ".  <font color=#008800>[fa-check-square-o]</font> Distortion" );
        }
    
        if ((swtch[i] & 4))
        {
            txtFlngSw[i].isChecked = true;
            txtFlngSw[i].SetHtml( ".  <font color=#008800>[fa-check-square-o]</font> Flanger" );
        }
        
         if ((swtch[i] & 8))
        {
            txtEchoSw[i].isChecked = true;
            txtEchoSw[i].SetHtml( ".  <font color=#008800>[fa-check-square-o]</font> Echo" );
        }
        
        if ((swtch[i] & 16))
        {
            txtRevbSw[i].isChecked = true;
            txtRevbSw[i].SetHtml( ".  <font color=#008800>[fa-check-square-o]</font> Reverb" );
        }
    }
}

// Set Knob assignments
function setKnobAssigns()
{
    if( rateKnob==0 ) spinRate.SetText("NA: (Not Assigned)");
    else if( rateKnob==1 ) spinRate.SetText("Tremolo: Rate");
    else if( rateKnob==2 ) spinRate.SetText("Tremolo: Depth");
    else if( rateKnob==3 ) spinRate.SetText("Distortion: Gain");
    else if( rateKnob==4 ) spinRate.SetText("Distortion: Mix");
    else if( rateKnob==5 ) spinRate.SetText("Flanger: Rate");
    else if( rateKnob==6 ) spinRate.SetText("Flanger: Range");
    else if( rateKnob==7 ) spinRate.SetText("Flanger: Color");
    else if( rateKnob==8 ) spinRate.SetText("Echo: Delay");
    else if( rateKnob==9 ) spinRate.SetText("Echo: Feedback");
    else if( rateKnob==10 ) spinRate.SetText("Reverb: Shimmer");
    else if( rateKnob==11 ) spinRate.SetText("Reverb: Depth");
    
    if( depthKnob==0 ) spinDepth.SetText("NA: (Not Assigned)");
    else if( depthKnob==1 ) spinDepth.SetText("Tremolo: Rate");
    else if( depthKnob==2 ) spinDepth.SetText("Tremolo: Depth");
    else if( depthKnob==3 ) spinDepth.SetText("Distortion: Gain");
    else if( depthKnob==4 ) spinDepth.SetText("Distortion: Mix");
    else if( depthKnob==5 ) spinDepth.SetText("Flanger: Rate");
    else if( depthKnob==6 ) spinDepth.SetText("Flanger: Range");
    else if( depthKnob==7 ) spinDepth.SetText("Flanger: Color");
    else if( depthKnob==8 ) spinDepth.SetText("Echo: Delay");
    else if( depthKnob==9 ) spinDepth.SetText("Echo: Feedback");
    else if( depthKnob==10 ) spinDepth.SetText("Reverb: Shimmer");
    else if( depthKnob==11 ) spinDepth.SetText("Reverb: Depth");
}

// Touch the phone '<' [Back] key
function OnBack()
{
    var yesNo = app.CreateYesNoDialog( "Exit Amplifier Settings App?" );
    yesNo.SetOnTouch( yesNo_OnTouch );
    yesNo.Show();
}

// Exit app dialog
function yesNo_OnTouch( result )
{
    if( result=="Yes" ) app.Exit();
}