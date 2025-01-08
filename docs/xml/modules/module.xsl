<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <!-- python3 -m http.server -d /parasol/docs/xml -->
  <xsl:output doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN" media-type="application/html+xml" encoding="utf-8" omit-xml-declaration="yes" indent="no"/>

  <xsl:template match="constants">
    <xsl:choose>
      <xsl:when test="const">
        <table class="table">
          <thead><tr><th>Name</th><th>Description</th></tr></thead>
          <tbody>
            <xsl:for-each select="const">
              <tr><th class="col-md-1"><xsl:value-of select="../@prefix"/>::<xsl:value-of select="@name"/></th><td><xsl:apply-templates select="."/></td></tr>
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
              <tr><th class="col-md-1"><xsl:value-of select="../@lookup"/>::<xsl:value-of select="@name"/></th><td><xsl:apply-templates select="."/></td></tr>
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
                <xsl:when test="../@lookup">
                  <tr><th class="col-md-1"><xsl:value-of select="../@lookup"/>::<xsl:value-of select="@name"/></th><td><xsl:apply-templates select="."/></td></tr>
                </xsl:when>
                <xsl:otherwise>
                  <tr><th class="col-md-1"><xsl:value-of select="@name"/></th><td><xsl:apply-templates select="."/></td></tr>
                </xsl:otherwise>
              </xsl:choose>
            </xsl:for-each>
          </tbody>
        </table>
      </xsl:when>
      <xsl:otherwise>
        <xsl:variable name="prefix"><xsl:value-of select="@lookup"/></xsl:variable>
        <table class="table">
          <thead><tr><th>Name</th><th>Description</th></tr></thead>
          <tbody>
            <xsl:for-each select="/book/types/constants[@lookup=$prefix]/const">
              <tr><th><xsl:value-of select="../@lookup"/>::<xsl:value-of select="@name"/></th><td><xsl:apply-templates select="."/></td></tr>
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

  <xsl:template match="st"> <!-- Struct reference -->
    <xsl:variable name="structName"><xsl:value-of select="node()"/></xsl:variable>
    <a data-toggle="tooltip"><xsl:attribute name="title"><xsl:value-of select="/book/structs/struct[name=$structName]/comment"/></xsl:attribute><xsl:attribute name="href">?page=struct-<xsl:value-of select="node()"/></xsl:attribute><xsl:value-of select="node()"/></a>
  </xsl:template>

  <xsl:template match="lk"> <!-- Type reference -->
    <xsl:variable name="typeName"><xsl:value-of select="node()"/></xsl:variable>
    <a data-toggle="tooltip"><xsl:attribute name="title"><xsl:value-of select="/book/structs/struct[name=$typeName]/comment"/></xsl:attribute><xsl:attribute name="href">?page=<xsl:value-of select="node()"/></xsl:attribute><xsl:value-of select="node()"/></a>
  </xsl:template>

  <xsl:template match="function">
    <xsl:choose>
      <xsl:when test="@module">
        <xsl:variable name="mod_name"><xsl:value-of select="@module"/></xsl:variable>
        <xsl:variable name="mod_lower" select="translate($mod_name,'ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz')"/>
        <a><xsl:attribute name="href"><xsl:value-of select="$mod_lower"/>.html?page=<xsl:value-of select="."/></xsl:attribute><xsl:value-of select="."/>()</a>
      </xsl:when>
      <xsl:otherwise>
        <a><xsl:attribute name="href">?page=<xsl:value-of select="."/></xsl:attribute><xsl:value-of select="."/>()</a>
      </xsl:otherwise>
    </xsl:choose>
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

  <xsl:template name="addGoogleTracking">
    <!-- Global site tag (gtag.js) - Google Analytics -->
	 <script async="async" src="https://www.googletagmanager.com/gtag/js?id=G-8254DG7MT6"><xsl:text> </xsl:text></script>
	 <script>
	   <xsl:text disable-output-escaping="yes">
	   window.dataLayer = window.dataLayer || [];
	   function gtag(){dataLayer.push(arguments);}
	   gtag('js', new Date());
      gtag('config', 'G-8254DG7MT6');</xsl:text>
	 </script>
  </xsl:template>

  <xsl:template match="/book">
    <html xml:lang="en">
      <head>
        <xsl:call-template name="addGoogleTracking"/>
        <meta charset="utf-8"/>
        <meta name="viewport" content="width=device-width, initial-scale=1"/>
        <!-- The above 2 meta tags *must* come first in the head; any other head content must come *after* these tags -->
        <meta name="description" content="Parasol Framework documentation, machine generated from source"/>
        <meta name="author" content="Paul Manias"/>
        <link rel="icon" href="/favicon.ico"/>
        <title>Parasol Framework Manual</title>
        <!-- Bootstrap core CSS -->
        <link href="../css/bootstrap.min.css" rel="stylesheet"/>
        <!-- Custom styles for this template -->
        <link href="../css/module-template.css" rel="stylesheet"/>
      </head>

      <body>
        <nav class="navbar navbar-expand-lg navbar-dark bg-dark fixed-top">
          <div class="container">
            <div class="navbar-header">
              <a class="navbar-brand" href="../index.html">Parasol Framework</a>
            </div>
            <div id="navbar" class="collapse navbar-collapse">
              <ul class="nav navbar-nav">
                <li class="nav-item dropdown active"><a class="nav-link dropdown-toggle" role="button" data-bs-toggle="dropdown" href="core.html">Modules</a>
                  <ul class="dropdown-menu dropdown-menu-dark">
                    <li><a class="dropdown-item" href="audio.html">Audio</a></li>
                    <li><a class="dropdown-item" href="core.html">Core</a></li>
                    <li><a class="dropdown-item" href="display.html">Display</a></li>
                    <li><a class="dropdown-item" href="fluid.html">Fluid</a></li>
                    <li><a class="dropdown-item" href="font.html">Font</a></li>
                    <li><a class="dropdown-item" href="network.html">Network</a></li>
                    <li><a class="dropdown-item" href="vector.html">Vector</a></li>
                  </ul>
                </li>
                <li class="nav-item"><a class="nav-link" href="classes/module.html">Classes</a></li>
                <li class="nav-item"><a class="nav-link" href="https://github.com/parasol-framework/parasol/wiki">Wiki</a></li>
              </ul>
            </div>
          </div>
        </nav>

        <div class="container"> <!-- Use container-fluid if you want full width -->
          <div class="row">
            <div class="col-sm-9">
              <!-- DEFAULT BODY -->
              <div class="docs-content" style="display:none;" id="default-page">
                <h1>Base Modules</h1>
                <p>The following modules are included in the standard distribution and can be loaded at run-time with <code>mod.load()</code> in Fluid or <code>LoadModule()</code> in C/C++.</p>
                <p>Use the navigation bar on the right to peruse the available functionality of the selected module.</p>
                <p>Beginners should start with the <a href="core.html">Core</a> module, which includes the bulk of Parasol's functionality.</p>
                <ul>
                  <li><a href="audio.html">Audio</a></li>
                  <li><a href="core.html">Core</a></li>
                  <li><a href="display.html">Display</a></li>
                  <li><a href="fluid.html">Fluid</a></li>
                  <li><a href="font.html">Font</a></li>
                  <li><a href="network.html">Network</a></li>
                  <li><a href="vector.html">Vector</a></li>
                </ul>
              </div>

              <!-- FUNCTION BODY -->
              <xsl:for-each select="function">
                <div class="docs-content" style="display:none;">
                  <xsl:attribute name="id"><xsl:value-of select="name"/></xsl:attribute>

                  <h2><xsl:value-of select="name"/>()</h2>
                  <p class="lead"><xsl:value-of select="comment"/></p>
                  <div class="card card-info">
                    <div class="card-header"><samp><xsl:value-of select="prototype"/></samp></div>

                    <xsl:choose>
                      <xsl:when test="input/param">
                        <div class="card-body" style="padding:0px">
                          <table class="table mb-3 thead-light">
                            <thead>
                              <tr><th class="col-md-1">Parameter</th><th>Description</th></tr>
                            </thead>
                            <tbody>
                              <xsl:for-each select="input/param">
                                <xsl:choose>
                                  <xsl:when test="@lookup">
                                    <tr><td><a><xsl:attribute name="onclick">showPage('<xsl:value-of select="@lookup"/>');</xsl:attribute><xsl:value-of select="@name"/></a></td><td><xsl:apply-templates select="."/></td></tr>
                                  </xsl:when>
                                  <xsl:otherwise>
                                    <tr><td><xsl:value-of select="@name"/></td><td><xsl:apply-templates select="."/></td></tr>
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
                            <tr><th class="col-md-1"><xsl:value-of select="@code"/></th><td><xsl:apply-templates select="."/></td></tr>
                          </xsl:for-each>
                        </tbody>
                      </table>
                    </xsl:when>
                    <xsl:when test="result">
                      <h3>Result</h3>
                      <p><xsl:apply-templates select="result/."/></p>
                    </xsl:when>
                  </xsl:choose>

                  <div class="footer copyright text-right"><xsl:value-of select="/book/info/name"/> module documentation © <xsl:value-of select="/book/info/copyright"/></div>
                </div>
              </xsl:for-each> <!-- End of function scan -->

              <!-- TYPES -->
              <xsl:for-each select="types/constants">
                <div class="docs-content" style="display:none;">
                  <xsl:attribute name="id"><xsl:value-of select="@lookup"/></xsl:attribute>
                  <h1><xsl:value-of select="@lookup"/> Type</h1>
                  <p class="lead"><xsl:apply-templates select="@comment"/></p>
                  <table class="table">
                    <thead><tr><th class="col-md-1">Name</th><th>Description</th></tr></thead>
                    <tbody>
                      <xsl:for-each select="const">
                        <tr><th><xsl:value-of select="../@lookup"/>::<xsl:value-of select="@name"/></th><td><xsl:apply-templates select="."/></td></tr>
                      </xsl:for-each>
                    </tbody>
                  </table>
                  <div class="footer copyright text-right"><xsl:value-of select="/book/info/name"/> module documentation © <xsl:value-of select="/book/info/copyright"/></div>
                </div>
              </xsl:for-each> <!-- End of type scan -->

              <!-- STRUCTURES -->
              <xsl:for-each select="structs/struct">
                <div class="docs-content" style="display:none;">
                  <xsl:attribute name="id">struct-<xsl:value-of select="@name"/></xsl:attribute>
                  <h1><xsl:value-of select="@name"/> Structure</h1>
                  <p class="lead"><xsl:apply-templates select="@comment"/></p>
                  <table class="table">
                    <thead><tr><th class="col-md-1">Field</th><th class="col-md-1">Type</th><th>Description</th></tr></thead>
                    <tbody>
                      <xsl:for-each select="field">
                        <tr>
                          <th><xsl:value-of select="@name"/></th>
                          <td><span class="text-nowrap"><xsl:value-of select="@type"/></span></td>
                          <td><xsl:apply-templates select="."/></td>
                        </tr>
                      </xsl:for-each>
                    </tbody>
                  </table>
                  <div class="footer copyright text-right"><xsl:value-of select="/book/info/name"/> module documentation © <xsl:value-of select="/book/info/copyright"/></div>
                </div>
              </xsl:for-each> <!-- End of struct scan -->

            </div> <!-- End of core content -->

            <!-- SIDEBAR -->
            <div class="col-sm-3">
              <div id="nav-tree">
                <h4><xsl:value-of select="info/name"/> Module</h4>
                  <!-- Classes -->
                  <div class="card mb-1">
                    <div class="card-header nav-classes">
                      <span class="badge rounded-pill"><xsl:value-of select="count(info/classes/class)"/></span>&#160;&#160;<a data-bs-toggle="collapse" data-parent="#accordion" href="#classes">Classes</a>
                    </div>
                    <div id="classes" class="collapse">
                      <div class="card-body">
                        <ul class="list-unstyled">
                          <xsl:for-each select="info/classes/class"><xsl:variable name="class_name"><xsl:value-of select="."/></xsl:variable><xsl:variable name="lower" select="translate($class_name,'ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz')"/><li><a class="card-link" role="button"><xsl:attribute name="href">classes/<xsl:value-of select="$lower"/>.html</xsl:attribute><xsl:value-of select="."/></a></li></xsl:for-each>
                        </ul>
                      </div>
                    </div>
                  </div>

                  <!-- Non-categorised functions -->
                  <xsl:if test="count(/book/*/category) = 0">
                    <div class="card mb-1">
                      <div class="card-header nav-functions">
                        <span class="badge rounded-pill"><xsl:value-of select="count(/book/function[not(category)])"/></span>&#160;&#160;<a data-bs-toggle="collapse" data-parent="#accordion" href="#Functions">Functions</a>
                      </div>

                      <div id="Functions" class="collapse">
                        <div class="card-body">
                          <ul class="list-unstyled">
                            <xsl:for-each select="/book/function[not(category)]"><li><a class="card-link" role="button"><xsl:attribute name="onclick">showPage('<xsl:value-of select="name"/>');</xsl:attribute><xsl:value-of select="name"/></a></li></xsl:for-each>
                          </ul>
                        </div>
                      </div>
                    </div>
                  </xsl:if>

                  <!-- Categorised functions -->
                  <xsl:for-each select="info/categories/category">
                    <xsl:variable name="category"><xsl:value-of select="."/></xsl:variable>
                    <xsl:variable name="id-category" select="translate($category,' ','_')"/>
                    <div class="card mb-1">
                      <div class="card-header nav-functions">
                        <span class="badge rounded-pill"><xsl:value-of select="count(/book/function[category=$category])"/></span>&#160;&#160;<a data-bs-toggle="collapse" data-parent="#accordion" href="#{$id-category}"><xsl:value-of select="."/></a>
                      </div>

                      <div id="{$id-category}" class="collapse">
                        <div class="card-body">
                          <ul class="list-unstyled">
                            <xsl:for-each select="/book/function[category=$category]"><li><a class="card-link" role="button"><xsl:attribute name="onclick">showPage('<xsl:value-of select="name"/>');</xsl:attribute><xsl:value-of select="name"/></a></li></xsl:for-each>
                          </ul>
                        </div>
                      </div>
                    </div>
                  </xsl:for-each>

                  <div class="card mb-1">
                    <div class="card-header nav-constants">
                      <a class="text-success" data-bs-toggle="collapse" data-parent="#accordion" href="#constants">Constants</a>
                    </div>
                    <div id="constants" class="collapse">
                      <div class="card-body">
                        <ul class="list-unstyled">
                          <xsl:for-each select="types/constants"><xsl:sort select="@lookup"/><li><a class="card-link" role="button"><xsl:attribute name="onclick">showPage('<xsl:value-of select="@lookup"/>');</xsl:attribute><xsl:value-of select="@lookup"/></a></li></xsl:for-each>
                        </ul>
                      </div>
                    </div>
                  </div>

                  <div class="card mb-1">
                    <div class="card-header nav-constants">
                      <a class="text-success" data-bs-toggle="collapse" data-parent="#accordion" href="#structures">Structures</a>
                    </div>
                    <div id="structures" class="collapse">
                      <div class="card-body">
                        <ul class="list-unstyled">
                          <xsl:for-each select="structs/struct"><xsl:sort select="@name"/><li><a class="card-link" role="button"><xsl:attribute name="onclick">showPage('struct-<xsl:value-of select="@name"/>');</xsl:attribute><xsl:value-of select="@name"/></a></li></xsl:for-each>
                        </ul>
                      </div>
                    </div>
                  </div>
              </div>
            </div>
          </div> <!-- row -->
        </div> <!-- container -->

        <script src="../js/bootstrap.bundle.min.js"></script>
        <script src="../js/base.js"></script>
        <script type="text/javascript">
var glCurrentMethod;

const ready = fn => document.readyState !== 'loading' ? fn() : document.addEventListener('DOMContentLoaded', fn);

ready(function(){
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
   var div = document.getElementById(Name);
   if (div) {
      if (glCurrentMethod) glCurrentMethod.style.display = "none"; // Hide previous method.
      div.style.display = "block"; // Show selected method.
      glCurrentMethod = div;

      if (!NoHistory) {
         history.pushState({ page: Name }, null, "?page=" + Name);
      }
      window.scrollTo(0, 0)
   }
   else console.log("Div for '" + Name + "' not found.");
}
         </script>
      </body>
    </html>
  </xsl:template>
</xsl:stylesheet>
