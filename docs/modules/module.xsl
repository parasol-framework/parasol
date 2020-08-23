<!-- xsltproc -o core.html module.xsl core.xml -->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN" media-type="application/html+xml" encoding="utf-8" omit-xml-declaration="yes" indent="no"/>

  <xsl:template match="constants">
    <xsl:choose>
      <xsl:when test="const">
        <table class="table">
          <thead><tr><th>Name</th><th>Description</th></tr></thead>
          <tbody>
            <xsl:for-each select="const">
              <tr><th class="col-md-1"><xsl:value-of select="../@prefix"/>_<xsl:value-of select="@name"/></th><td><xsl:value-of select="."/></td></tr>
            </xsl:for-each>
          </tbody>
        </table>
      </xsl:when>
      <xsl:otherwise>
        <xsl:variable name="prefix"><xsl:value-of select="@prefix"/></xsl:variable>
        <table class="table">
          <thead><tr><th>Name</th><th>Description</th></tr></thead>
          <tbody>
            <xsl:for-each select="/book/types/constants[@lookup=$prefix]/const">
              <tr><th class="col-md-1"><xsl:value-of select="../@lookup"/>_<xsl:value-of select="@name"/></th><td><xsl:value-of select="."/></td></tr>
            </xsl:for-each>
          </tbody>
        </table>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="types">
    <xsl:choose>
      <xsl:when test="type">
        <table class="table">
          <thead><tr><th>Name</th><th>Description</th></tr></thead>
          <tbody>
            <xsl:for-each select="type">
              <xsl:choose>
                <xsl:when test="../@prefix">
                  <tr><th class="col-md-1"><xsl:value-of select="../@prefix"/>_<xsl:value-of select="@name"/></th><td><xsl:value-of select="."/></td></tr>
                </xsl:when>
                <xsl:otherwise>
                  <tr><th class="col-md-1"><xsl:value-of select="@name"/></th><td><xsl:value-of select="."/></td></tr>
                </xsl:otherwise>
              </xsl:choose>
            </xsl:for-each>
          </tbody>
        </table>
      </xsl:when>
      <xsl:otherwise>
        <xsl:variable name="prefix"><xsl:value-of select="@prefix"/></xsl:variable>
        <table class="table">
          <thead><tr><th>Name</th><th>Description</th></tr></thead>
          <tbody>
            <xsl:for-each select="/book/types/constants[@lookup=$prefix]/const">
              <tr><th><xsl:value-of select="../@lookup"/>_<xsl:value-of select="@name"/></th><td><xsl:value-of select="."/></td></tr>
            </xsl:for-each>
          </tbody>
        </table>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="text()"><xsl:value-of select="."/></xsl:template>

  <xsl:template match="p">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates select="*|text()" />
    </xsl:copy>
  </xsl:template>

  <xsl:template match="list">
    <xsl:choose>
      <xsl:when test="@type='ordered'">
        <ol><xsl:apply-templates select="*|node()"/></ol>
      </xsl:when>
      <xsl:otherwise>
        <ul><xsl:apply-templates select="*|node()"/></ul>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="li">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates select="*|text()" />
    </xsl:copy>
  </xsl:template>

  <xsl:template match="include">
    <code><xsl:value-of select="."/></code>
  </xsl:template>

  <xsl:template match="header">
    <h3><xsl:value-of select="."/></h3>
  </xsl:template>

  <xsl:template match="code">
    <xsl:copy-of select="."/>
  </xsl:template>

  <xsl:template match="pre">
    <xsl:copy-of select="."/>
  </xsl:template>

  <xsl:template match="/book">
    <html xml:lang="en">
      <head>
        <meta charset="utf-8"/>
        <meta http-equiv="X-UA-Compatible" content="IE=edge"/>
        <meta name="viewport" content="width=device-width, initial-scale=1"/>
        <!-- The above 3 meta tags *must* come first in the head; any other head content must come *after* these tags -->
        <meta name="description" content="Parasol Framework documentation, machine generated from source"/>
        <meta name="author" content="Paul Manias"/>
        <link rel="icon" href="/favicon.ico"/>
        <title>Parasol Framework Manual</title>
        <!-- Bootstrap core CSS -->
        <link href="../css/bootstrap.min.css" rel="stylesheet"/>
        <!-- Custom styles for this template -->
        <link href="../css/module-template.css" rel="stylesheet"/>

        <!-- HTML5 shim and Respond.js for IE8 support of HTML5 elements and media queries -->
        <!--[if lt IE 9]>
          <script src="https://oss.maxcdn.com/html5shiv/3.7.3/html5shiv.min.js"></script>
          <script src="https://oss.maxcdn.com/respond/1.4.2/respond.min.js"></script>
        <![endif]-->
      </head>

      <body>
        <nav class="navbar navbar-inverse navbar-fixed-top">
          <div class="container">
            <div class="navbar-header">
              <button type="button" class="navbar-toggle collapsed" data-toggle="collapse" data-target="#navbar" aria-expanded="false" aria-controls="navbar">
                <span class="sr-only">Toggle navigation</span>
                <span class="icon-bar"></span>
                <span class="icon-bar"></span>
                <span class="icon-bar"></span>
              </button>
              <a class="navbar-brand" href="/index.html">Parasol Framework</a>
            </div>
            <div id="navbar" class="collapse navbar-collapse">
              <ul class="nav navbar-nav">
                <li class="active"><a href="core.xml">Modules</a></li>
                <li><a href="/modules/classes/module.xml">Classes</a></li>
                <li><a href="/modules/fluid.xml">Fluid</a></li>
              </ul>
            </div> <!-- nav-collapse -->
          </div>
        </nav>

        <div class="container"> <!-- Use container-fluid if you want full width -->
          <div class="row">
            <div class="col-sm-9">
              <div class="docs-content" style="display:none;" id="default-page">
                <h1>Base Modules</h1>
                <p>The following modules are included in the standard distribution and can be loaded at run-time with <samp>mod.load()</samp> in Fluid or <samp>LoadModule()</samp> in C/C++.</p>
                <p>Use the navigation bar on the right to peruse the available functionality of the selected module.</p>
                <p>Beginners should start with the Core module, which includes the bulk of Parasol's functionality.</p>
                <ul>
                  <li><a href="audio.xml">Audio</a></li>
                  <li><a href="core.xml">Core</a></li>
                  <li><a href="display.xml">Display</a></li>
                  <li><a href="document.xml">Document</a></li>
                  <li><a href="fluid.xml">Fluid</a></li>
                  <li><a href="font.xml">Font</a></li>
                  <li><a href="iconserver.xml">IconServer</a></li>
                  <li><a href="network.xml">Network</a></li>
                  <li><a href="surface.xml">Surface</a></li>
                  <li><a href="vector.xml">Vector</a></li>
                </ul>
              </div>

              <!-- FUNCTIONS -->
              <xsl:for-each select="function">
                <div class="docs-content" style="display:none;">
                  <xsl:attribute name="id"><xsl:value-of select="name"/></xsl:attribute>

                  <h1><xsl:value-of select="name"/>()</h1>
                  <p class="lead"><xsl:value-of select="comment"/></p>
                  <div class="panel panel-info">
                    <div class="panel-heading"><samp><xsl:value-of select="prototype"/></samp></div>

                    <xsl:choose>
                      <xsl:when test="input/param">
                        <div class="panel-body">
                          <table class="table" style="border: 4px; margin-bottom: 0px; border: 0px; border-bottom: 0px;">
                          <thead>
                            <tr><th class="col-md-1">Parameter</th><th>Description</th></tr>
                          </thead>
                            <tbody>
                              <xsl:for-each select="input/param">
                                <xsl:choose>
                                  <xsl:when test="@lookup">
                                    <tr><td><a><xsl:attribute name="onclick">showPage('<xsl:value-of select="@lookup"/>');</xsl:attribute><xsl:value-of select="@name"/></a></td><td><xsl:value-of select="."/></td></tr>
                                  </xsl:when>
                                  <xsl:otherwise>
                                    <tr><td><xsl:value-of select="@name"/></td><td><xsl:value-of select="."/></td></tr>
                                  </xsl:otherwise>
                                </xsl:choose>
                              </xsl:for-each>
                            </tbody>
                          </table>
                        </div>
                      </xsl:when>
                    </xsl:choose>
                  </div>

                  <h3>Description</h3>
                  <xsl:for-each select="description">
                    <xsl:apply-templates/>
                  </xsl:for-each>

                  <xsl:choose>
                    <xsl:when test="result/error">
                      <h3>Error Codes</h3>
                      <table class="table table-sm borderless">
                        <tbody>
                          <xsl:for-each select="result/error">
                            <tr><th class="col-md-1"><xsl:value-of select="@code"/></th><td><xsl:value-of select="."/></td></tr>
                          </xsl:for-each>
                        </tbody>
                      </table>
                    </xsl:when>
                    <xsl:when test="result">
                      <h3>Result</h3>
                      <p><xsl:value-of select="result/."/></p>
                    </xsl:when>
                  </xsl:choose>

                  <div class="footer copyright"><xsl:value-of select="/book/info/name"/> module documentation © <xsl:value-of select="/book/info/copyright"/></div>
                </div>
              </xsl:for-each> <!-- End of function scan -->

              <!-- TYPES -->
              <xsl:for-each select="types/constants">
                <div class="docs-content" style="display:none;">
                  <xsl:attribute name="id"><xsl:value-of select="@lookup"/></xsl:attribute>
                  <h1><xsl:value-of select="@lookup"/> Type</h1>
                  <p class="lead"><xsl:value-of select="@comment"/></p>
                  <table class="table" style="border: 4px; margin-bottom: 0px; border: 0px; border-bottom: 0px;">
                    <thead><tr><th class="col-md-1">Name</th><th>Description</th></tr></thead>
                    <tbody>
                      <xsl:for-each select="const">
                        <tr><td><xsl:value-of select="../@lookup"/>_<xsl:value-of select="@name"/></td><td><xsl:value-of select="."/></td></tr>
                      </xsl:for-each>
                    </tbody>
                  </table>
                  <div class="footer copyright"><xsl:value-of select="/book/info/name"/> module documentation © <xsl:value-of select="/book/info/copyright"/></div>
                </div>
              </xsl:for-each> <!-- End of type scan -->

              <!-- STRUCTURES -->
              <xsl:for-each select="structs/struct">
                <div class="docs-content" style="display:none;">
                  <xsl:attribute name="id">struct-<xsl:value-of select="@name"/></xsl:attribute>
                  <h1><xsl:value-of select="@name"/> Structure</h1>
                  <p class="lead"><xsl:value-of select="@comment"/></p>
                  <table class="table" style="border: 4px; margin-bottom: 0px; border: 0px; border-bottom: 0px;">
                    <thead><tr><th class="col-md-1">Field</th><th class="col-md-1">Type</th><th>Description</th></tr></thead>
                    <tbody>
                      <xsl:for-each select="field">
                        <tr>
                          <td><xsl:value-of select="@name"/></td>
                          <td><span class="text-nowrap"><xsl:value-of select="@type"/></span></td>
                          <td><xsl:value-of select="."/></td>
                        </tr>
                      </xsl:for-each>
                    </tbody>
                  </table>
                  <div class="footer copyright"><xsl:value-of select="/book/info/name"/> module documentation © <xsl:value-of select="/book/info/copyright"/></div>
                </div>
              </xsl:for-each> <!-- End of type scan -->

            </div> <!-- End of core content -->

            <!-- SIDEBAR -->
            <div class="col-sm-3">
              <div id="nav-tree">
                <h3><xsl:value-of select="info/name"/> Module</h3>

                <div class="panel-group" id="accordion">
                  <div class="panel panel-info">
                    <div class="panel-heading">
                      <h4 class="panel-title"><span class="badge" style="display:inline-block; width:30px; background-color:#fff; color:#666"><xsl:value-of select="count(info/classes/class)"/></span>&#160;&#160;<a data-toggle="collapse" data-parent="#accordion" href="#classes">Classes</a></h4>
                      <!--<h4 class="panel-title"><a data-toggle="collapse" data-parent="#accordion" href="#classes">Classes</a></h4>-->
                    </div>
                    <div id="classes" class="panel-collapse collapse">
                      <div class="panel-body">
                        <ul class="list-unstyled">
                          <xsl:for-each select="info/classes/class"><li><a><xsl:attribute name="href">classes/<xsl:value-of select="."/>.xml</xsl:attribute><xsl:value-of select="."/></a></li></xsl:for-each>
                        </ul>
                      </div>
                    </div>
                  </div>

                  <xsl:if test="count(/book/*/category) = 0">
                    <div class="panel panel-primary">
                      <div class="panel-heading">
                        <h4 class="panel-title"><span class="badge" style="display:inline-block; width:30px"><xsl:value-of select="count(/book/function[not(category)])"/></span>&#160;&#160;<a data-toggle="collapse" data-parent="#accordion" href="#Functions">Functions</a></h4>
                      </div>

                      <div id="Functions" class="panel-collapse collapse">
                        <div class="panel-body">
                          <ul class="list-unstyled">
                            <xsl:for-each select="/book/function[not(category)]"><li><a><xsl:attribute name="onclick">showPage('<xsl:value-of select="name"/>');</xsl:attribute><xsl:value-of select="name"/></a></li></xsl:for-each>
                          </ul>
                        </div>
                      </div>
                    </div>
                  </xsl:if>

                  <xsl:for-each select="info/categories/category">
                    <xsl:variable name="category"><xsl:value-of select="."/></xsl:variable>
                    <xsl:variable name="id-category" select="translate($category,' ','_')"/>
                    <div class="panel panel-primary">
                      <div class="panel-heading">
                        <h4 class="panel-title"><span class="badge" style="display:inline-block; width:30px"><xsl:value-of select="count(/book/function[category=$category])"/></span>&#160;&#160;<a data-toggle="collapse" data-parent="#accordion" href="#{$id-category}"><xsl:value-of select="."/></a></h4>
                        <!--<h4 class="panel-title"><a data-toggle="collapse" data-parent="#accordion" href="#{$id-category}"><xsl:value-of select="."/></a></h4>-->
                      </div>

                      <div id="{$id-category}" class="panel-collapse collapse">
                        <div class="panel-body">
                          <ul class="list-unstyled">
                            <xsl:for-each select="/book/function[category=$category]"><li><a><xsl:attribute name="onclick">showPage('<xsl:value-of select="name"/>');</xsl:attribute><xsl:value-of select="name"/></a></li></xsl:for-each>
                          </ul>
                        </div>
                      </div>
                    </div>
                  </xsl:for-each>

                  <div class="panel panel-success">
                    <div class="panel-heading">
                      <h4 class="panel-title"><a data-toggle="collapse" data-parent="#accordion" href="#structures">Structures</a></h4>
                    </div>
                    <div id="structures" class="panel-collapse collapse">
                      <div class="panel-body">
                        <ul class="list-unstyled">
                          <xsl:for-each select="structs/struct"><xsl:sort select="@name"/><li><a><xsl:attribute name="onclick">showPage('struct-<xsl:value-of select="@name"/>');</xsl:attribute><xsl:value-of select="@name"/></a></li></xsl:for-each>
                        </ul>
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div> <!-- row -->
        </div> <!-- container -->

        <script src="https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js"></script>
        <script src="../js/bootstrap.min.js"></script>
        <script src="../js/base.js"></script>
        <script type="text/javascript">
var glCurrentMethod;

$(document).ready(function() {
   glCurrentMethod = document.getElementById("Introduction");

   var page = glParameters["page"];
   if (isEmpty(page)) page = glParameters["function"];

   if (isEmpty(page)) showPage("default-page", true);
   else showPage(page, true);

   window.onpopstate = popState;
});

function popState(event) {
   console.log("popState() to " + JSON.stringify(event.state));

   state = event.state
   if (!state) state = { page: 'default-page' }

   var div
   if (state.page) div = document.getElementById(state.page);
   else div =  document.getElementById('default-page');

   if (div) {
      if (glCurrentMethod) glCurrentMethod.style.display = "none"; // Hide previous method.
      div.style.display = "block"; // Show selected method.
      glCurrentMethod = div;
   }
   else console.log("Div for '" + state.page + "' not found.");
}

function showPage(Name, NoHistory)
{
   console.log('showPage() ' + Name);

   var div = document.getElementById(Name);
   if (div) {
      if (glCurrentMethod) glCurrentMethod.style.display = "none"; // Hide previous method.
      div.style.display = "block"; // Show selected method.
      glCurrentMethod = div;

      if (!NoHistory) {
         history.pushState({ page: Name }, null, "?page=" + Name);
      }
   }
   else console.log("Div for '" + Name + "' not found.");
}
         </script>
      </body>
    </html>
  </xsl:template>
</xsl:stylesheet>
