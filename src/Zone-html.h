/*

*/
#ifndef ZONE_HTML_H_
#define ZONE_HTML_H_

const char info_zone_html[] PROGMEM = R"rawliteral(
<div class="col-sm-3">
  <div class="panel panel-primary">
    <div class="panel-heading"><h3 class="panel-title">Zone #Z#</h3></div>
    <div class="panel-body"><p id="info_zone#Z#">?</p></div>
  </div>
</div>
)rawliteral";

const char manu_zone_html[] PROGMEM = R"rawliteral(
<div class="row">
  <div class="col-xs-4 col-md-4">
    <h2 class="text-left">EV #Z#
      <span class="label label-info" id="EV#Z#_etat">OFF</span>
    </h2>
  </div>
  <div class="col-xs-4 col-md-4">
    <div class="button btn btn-success btn-lg" id="EV#Z#_On" type="button">ON</div>
  </div>
  <div class="col-xs-4 col-md-4">
    <div class="button btn btn-danger btn-lg" id="EV#Z#_Off" type="button">OFF</div>
  </div>
</div>
)rawliteral";

const char cnf_zone_html[] PROGMEM = R"rawliteral(
<FORM action="/cnfzone" id="formz#Z#">
  <input type="hidden" name="zone" value="#z#">
  <div class="bs-callout bs-callout-info"><h4>Zone #Z#</h4>
    <div class="row">
      <div class="col-md-1">
        <div class="input-group">
          <span class="input-group-addon">
            <input type="checkbox" aria-label="lundi" name="DAY1" %CNFZ_DAY1%>
          </span>
          <span class="form-control" aria-label="lundi">Lundi</span>
        </div>
      </div>
      <div class="col-md-1">
        <div class="input-group">
          <span class="input-group-addon">
            <input type="checkbox" aria-label="mardi" name="DAY2" %CNF_DAY2%>
          </span>
          <span class="form-control" aria-label="mardi">Mardi</span>
        </div>
      </div>
      <div class="col-md-1">
        <div class="input-group">
          <span class="input-group-addon">
            <input type="checkbox" aria-label="mercredi" name="DAY3" %CNFZ_DAY3%>
          </span>
          <span class="form-control" aria-label="mercredi">Mercredi</span>
        </div>
      </div>
      <div class="col-md-1">
        <div class="input-group">
          <span class="input-group-addon">
            <input type="checkbox" aria-label="jeudi" name="DAY4" %CNFZ_DAY4%>
          </span>
          <span class="form-control" aria-label="jeudi">Jeudi</span>
        </div>
      </div>
      <div class="col-md-1">
        <div class="input-group">
          <span class="input-group-addon">
            <input type="checkbox" aria-label="vendredi" name="DAY5" %CNFZ_DAY5%>
          </span>
          <span class="form-control" aria-label="vendredi">Vendredi</span>
        </div>
      </div>
      <div class="col-md-1">
        <div class="input-group">
          <span class="input-group-addon">
            <input type="checkbox" aria-label="samedi" name="DAY6" %CNFZ_DAY6%>
          </span>
          <span class="form-control" aria-label="samedi">Samedi</span>
        </div>
      </div>
      <div class="col-md-1">
        <div class="input-group">
          <span class="input-group-addon">
            <input type="checkbox" aria-label="dimanche" name="DAY7" %CNFZ_DAY7%>
          </span>
          <span class="form-control" aria-label="dimanche">Dimanche</span>
        </div>
      </div>
      <div class="col-md-5">
        <div class="input-group">
          <span class="input-group-addon" id="cnf-z#Z#-moisture">Humidit&eacute mini %</span>
          <input type="range" class="form-control" aria-describedby="cnf-z#Z#-moisture" name="HUM" min="0" max="100" value="%CNFZ_HUM%">
        </div>
      </div>
    </div>
    <br>
    <div class="row">
<!--CNFCYCLE-->
    </div>
    <div class="row">
      <div class="col-md-1">
        <button type="submit" class="btn btn-primary">Valide</button>
      </div>
      <div class="col-md-11">
        <p class="bg-success" style="opacity: 0" id="resultz#Z#"><strong>Enregistr&eacute;!</strong></p>
      </div>
    </div>
  </div>
</form>
)rawliteral";

const char cnf_zone_cycle_html[] PROGMEM = R"rawliteral(
<div class="col-md-3">
  <h4 class="panel panel-primary">Cycle #C#</h4>
  <div class="input-group">
    <span class="input-group-addon" id="cnf_cycle#C#_debhr">Heure d&eacute;but</span>
    <input type="number" class="form-control" aria-describedby="cnf_cycle#C#_debhr" name="C#C#HD" value="%CNFZC_HD%">
    <span class="input-group-addon">0..23</span>
  </div>
  <div class="input-group">
    <span class="input-group-addon" id="cnf_cycle#C#_debmn">Minute d&eacute;but</span>
    <input type="number" class="form-control" aria-describedby="cnf_cycle#C#_debmn" name="C#C#MD" value="%CNFZC_MD%">
    <span class="input-group-addon">0..59</span>
  </div>
  <div class="input-group">
    <span class="input-group-addon" id="cnf_cycle#C#_duree">Dur&eacute;e</span>
    <input type="number" class="form-control" aria-describedby="cnf_cycle#C#_duree" name="C#C#D" value="%CNFZC_D%">
    <span class="input-group-addon">0..240</span>
  </div>
</div>
)rawliteral";

const char zone_js[] PROGMEM = R"rawliteral(
    $('#EV#Z#_On').click(function(){ setBouton('EV#Z#','1'); });
    $('#EV#Z#_Off').click(function(){ setBouton('EV#Z#','0'); });
    $('#formz#Z#').submit(function( event ) {
      console.log('Form submit');
      event.preventDefault();
      var posting = $.post(event.currentTarget.action, $(this).serialize() );
      posting.done(function( data ) {
        $("#resultz#Z#").fadeTo(100, 1);
        window.setTimeout(function() {$("#resultz#Z#").fadeTo(500, 0)}, 2000);
        console.log(data);
      });
    });
)rawliteral";

#endif // ZONE_HTMLH_
