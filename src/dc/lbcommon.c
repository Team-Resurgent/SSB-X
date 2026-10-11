/* lbcommon.c -- lb/lbcommon.c's table trigonometry, verbatim: the
 * 1024-entry quarter-wave sine table (lbcommon.c:18-276) and lbCommonSin,
 * lbCommonCos and lbCommonTan over it (lbcommon.c:321-388). The battle
 * camera (gm/gmcamera.c) angles its eye through these, and a rounded
 * sine here would put the eye somewhere the N64 does not: the table
 * quantises the angle to 1/4096 of a turn and the game's arithmetic
 * follows from that, so the C library's sinf is not a substitute.
 *
 * The rest of lb/lbcommon.c is the fighter parts builder, the sprite drawing
 * and the DObj/MObj helpers, which the port reaches other ways
 * (src/dc/objmodel.c) or has not reached yet. This file is a direct copy of
 * the three functions and the table; the text between the markers is the
 * decomp's, taken by scripts at the port's revision of the submodule.
 * lbCommonMakePositionFGM is at the end, verbatim, and the sprite renderer
 * after it (lbcommon.c:2218-3007), ported.
 */
#include "lbcommon.h"

/* n_env.c's start-and-get-handle and set-balance calls, which no decomp
 * header declares; src/dc/sysshim.c defines them over the FGM engine */
alSoundEffect *func_80026A10_27610(u32 fgm_id);
void func_800267F4_273F4(alSoundEffect *sfx);

/* ---- lbcommon.c:17-276 dLBCommonSinLookup, verbatim ---------------- */
// 0x800D4CA0
f32 dLBCommonSinLookup[/* */] =
{
	0.000000000000000, 0.001534000039101, 0.003068000078201, 0.004602000117302,
	0.006136000156403, 0.007670000195503, 0.009204000234604, 0.010738000273705,
	0.012272000312805, 0.013805000111461, 0.015339000150561, 0.016873000189662,
	0.018407000228763, 0.019940000027418, 0.021474000066519, 0.023008000105619,
	0.024540999904275, 0.026074999943376, 0.027607999742031, 0.029141999781132,
	0.030674999579787, 0.032207999378443, 0.033741001039743, 0.035273998975754,
	0.036807000637054, 0.038339998573065, 0.039873000234365, 0.041405998170376,
	0.042938001453876, 0.044470999389887, 0.046002998948097, 0.047534998506308,
	0.049068000167608, 0.050599999725819, 0.052131999284029, 0.053663998842239,
	0.055195000022650, 0.056726999580860, 0.058258000761271, 0.059790000319481,
	0.061321001499891, 0.062852002680302, 0.064383000135422, 0.065912999212742,
	0.067443996667862, 0.068974003195763, 0.070505000650883, 0.072034999728203,
	0.073564998805523, 0.075093999505043, 0.076623998582363, 0.078152999281883,
	0.079681999981403, 0.081211000680923, 0.082740001380444, 0.084269002079964,
	0.085796996951103, 0.087325997650623, 0.088853999972343, 0.090380996465683,
	0.091908998787403, 0.093436002731323, 0.094962999224663, 0.096490003168583,
	0.098016999661922, 0.099544003605843, 0.101070001721382, 0.102595999836922,
	0.104121997952461, 0.105646997690201, 0.107171997427940, 0.108696997165680,
	0.110221996903419, 0.111746996641159, 0.113270998001099, 0.114794999361038,
	0.116319000720978, 0.117842003703117, 0.119364999234676, 0.120888002216816,
	0.122410997748375, 0.123933002352715, 0.125455006957054, 0.126976996660233,
	0.128498002886772, 0.130018994212151, 0.131540000438690, 0.133061006665230,
	0.134580999612808, 0.136100992560387, 0.137620002031326, 0.139138996601105,
	0.140658006072044, 0.142177000641823, 0.143694996833801, 0.145212993025780,
	0.146730005741119, 0.148248001933098, 0.149764999747276, 0.151280999183655,
	0.152796998620033, 0.154312998056412, 0.155827999114990, 0.157343000173569,
	0.158858001232147, 0.160372003912926, 0.161886006593704, 0.163399994373322,
	0.164912998676300, 0.166426002979279, 0.167937994003296, 0.169449999928474,
	0.170962005853653, 0.172472998499870, 0.173984006047249, 0.175494000315666,
	0.177003994584084, 0.178514003753662, 0.180022999644279, 0.181531995534897,
	0.183039993047714, 0.184548005461693, 0.186055004596710, 0.187562003731728,
	0.189069002866745, 0.190575003623962, 0.192080006003380, 0.193586006760597,
	0.195089995861053, 0.196594998240471, 0.198098003864288, 0.199601992964745,
	0.201104998588562, 0.202607005834579, 0.204108998179436, 0.205610007047653,
	0.207111001014709, 0.208611994981766, 0.210112005472183, 0.211611002683640,
	0.213109999895096, 0.214608997106552, 0.216106995940208, 0.217603996396065,
	0.219100996851921, 0.220597997307777, 0.222093999385834, 0.223589003086090,
	0.225084006786346, 0.226577997207642, 0.228072002530098, 0.229564994573593,
	0.231058001518250, 0.232549995183945, 0.234042003750801, 0.235532999038696,
	0.237023994326591, 0.238514006137848, 0.240003004670143, 0.241492003202438,
	0.242980003356934, 0.244468003511429, 0.245955005288124, 0.247442007064819,
	0.248927995562553, 0.250413000583649, 0.251897990703583, 0.253381997346878,
	0.254866003990173, 0.256348997354507, 0.257831007242203, 0.259312987327576,
	0.260794013738632, 0.262275010347366, 0.263754993677139, 0.265233993530273,
	0.266712993383408, 0.268191009759903, 0.269668012857437, 0.271144986152649,
	0.272621005773544, 0.274096995592117, 0.275572001934052, 0.277045994997025,
	0.278519988059998, 0.279992997646332, 0.281464993953705, 0.282936990261078,
	0.284408003091812, 0.285878002643585, 0.287346988916397, 0.288816004991531,
	0.290284991264343, 0.291752010583878, 0.293219000101089, 0.294685006141663,
	0.296151012182236, 0.297616004943848, 0.299080014228821, 0.300543010234833,
	0.302006006240845, 0.303467988967896, 0.304928988218307, 0.306389987468719,
	0.307850003242493, 0.309309005737305, 0.310766994953156, 0.312225013971329,
	0.313681989908218, 0.315138012170792, 0.316592991352081, 0.318048000335693,
	0.319501996040344, 0.320955008268356, 0.322407990694046, 0.323859006166458,
	0.325309991836548, 0.326759994029999, 0.328209996223450, 0.329658001661301,
	0.331106007099152, 0.332552999258041, 0.333999991416931, 0.335444986820221,
	0.336890012025833, 0.338333994150162, 0.339776992797852, 0.341219007968903,
	0.342660993337631, 0.344101011753082, 0.345541000366211, 0.346980005502701,
	0.348419010639191, 0.349855989217758, 0.351292997598648, 0.352728992700577,
	0.354164004325867, 0.355598002672195, 0.357030987739563, 0.358462989330292,
	0.359894990921021, 0.361326009035110, 0.362756013870239, 0.364185005426407,
	0.365613013505936, 0.367040008306503, 0.368467003107071, 0.369892001152039,
	0.371316999197006, 0.372741013765335, 0.374163985252380, 0.375586003065109,
	0.377007007598877, 0.378428012132645, 0.379846990108490, 0.381265997886658,
	0.382683008909225, 0.384099990129471, 0.385515987873077, 0.386931002140045,
	0.388345003128052, 0.389757990837097, 0.391169995069504, 0.392581999301910,
	0.393992006778717, 0.395401000976562, 0.396809995174408, 0.398218005895615,
	0.399623990058899, 0.401030004024506, 0.402435004711151, 0.403838008642197,
	0.405241012573242, 0.406643003225327, 0.408044010400772, 0.409444004297256,
	0.410843014717102, 0.412241011857986, 0.413637995719910, 0.415033996105194,
	0.416429996490479, 0.417824000120163, 0.419216990470886, 0.420608997344971,
	0.421999990940094, 0.423390001058578, 0.424780011177063, 0.426167994737625,
	0.427554994821548, 0.428941011428833, 0.430326014757156, 0.431710988283157,
	0.433093994855881, 0.434475988149643, 0.435856997966766, 0.437236994504929,
	0.438616007566452, 0.439994007349014, 0.441370993852615, 0.442746996879578,
	0.444121986627579, 0.445495992898941, 0.446868985891342, 0.448240995407104,
	0.449611008167267, 0.450980991125107, 0.452349990606308, 0.453716993331909,
	0.455083996057510, 0.456449002027512, 0.457812994718552, 0.459176987409592,
	0.460539013147354, 0.461899995803833, 0.463259994983673, 0.464619010686874,
	0.465977013111115, 0.467332988977432, 0.468688994646072, 0.470043003559113,
	0.471397012472153, 0.472748994827271, 0.474099993705750, 0.475450009107590,
	0.476799011230469, 0.478147000074387, 0.479494005441666, 0.480839014053345,
	0.482183992862701, 0.483527004718781, 0.484869003295898, 0.486209988594055,
	0.487549990415573, 0.488889008760452, 0.490227013826370, 0.491562992334366,
	0.492897987365723, 0.494231998920441, 0.495564997196198, 0.496897011995316,
	0.498228013515472, 0.499556988477707, 0.500885009765625, 0.502211987972260,
	0.503538012504578, 0.504863023757935, 0.506187021732330, 0.507508993148804,
	0.508830010890961, 0.510150015354156, 0.511469006538391, 0.512785971164703,
	0.514102995395660, 0.515417993068695, 0.516731977462769, 0.518045008182526,
	0.519356012344360, 0.520666003227234, 0.521974980831146, 0.523283004760742,
	0.524590015411377, 0.525894999504089, 0.527198970317841, 0.528501987457275,
	0.529803991317749, 0.531104028224945, 0.532402992248535, 0.533701002597809,
	0.534997999668121, 0.536293029785156, 0.537586987018585, 0.538879990577698,
	0.540171980857849, 0.541462004184723, 0.542751014232635, 0.544039011001587,
	0.545324981212616, 0.546609997749329, 0.547894001007080, 0.549176990985870,
	0.550458014011383, 0.551738023757935, 0.553017020225525, 0.554293990135193,
	0.555570006370544, 0.556845009326935, 0.558118999004364, 0.559391021728516,
	0.560661971569061, 0.561931014060974, 0.563198983669281, 0.564465999603271,
	0.565732002258301, 0.566995978355408, 0.568259000778198, 0.569521009922028,
	0.570780992507935, 0.572040021419525, 0.573297023773193, 0.574553012847900,
	0.575807988643646, 0.577062010765076, 0.578314006328583, 0.579564988613129,
	0.580814003944397, 0.582062005996704, 0.583308994770050, 0.584554016590118,
	0.585798025131226, 0.587040007114410, 0.588281989097595, 0.589520990848541,
	0.590759992599487, 0.591997027397156, 0.593231976032257, 0.594466984272003,
	0.595699012279510, 0.596930980682373, 0.598160982131958, 0.599389016628265,
	0.600615978240967, 0.601841986179352, 0.603066980838776, 0.604290008544922,
	0.605511009693146, 0.606730997562408, 0.607949972152710, 0.609166979789734,
	0.610382974147797, 0.611597001552582, 0.612810015678406, 0.614022016525269,
	0.615231990814209, 0.616439998149872, 0.617646992206573, 0.618852972984314,
	0.620056986808777, 0.621259987354279, 0.622461020946503, 0.623660981655121,
	0.624859988689423, 0.626056015491486, 0.627251982688904, 0.628445982933044,
	0.629638016223907, 0.630828976631165, 0.632018983364105, 0.633207023143768,
	0.634392976760864, 0.635577976703644, 0.636762022972107, 0.637943983078003,
	0.639123976230621, 0.640303015708923, 0.641480982303619, 0.642656981945038,
	0.643832027912140, 0.645004987716675, 0.646175980567932, 0.647346019744873,
	0.648513972759247, 0.649680972099304, 0.650847017765045, 0.652010977268219,
	0.653173029422760, 0.654334008693695, 0.655493021011353, 0.656651020050049,
	0.657806992530823, 0.658960998058319, 0.660113990306854, 0.661266028881073,
	0.662415981292725, 0.663564026355743, 0.664710998535156, 0.665856003761292,
	0.666999995708466, 0.668142020702362, 0.669282972812653, 0.670422017574310,
	0.671558976173401, 0.672694981098175, 0.673829019069672, 0.674961984157562,
	0.676092982292175, 0.677222013473511, 0.678349971771240, 0.679476022720337,
	0.680601000785828, 0.681724011898041, 0.682846009731293, 0.683965027332306,
	0.685083985328674, 0.686200022697449, 0.687314987182617, 0.688428997993469,
	0.689540982246399, 0.690650999546051, 0.691758990287781, 0.692866027355194,
	0.693971991539001, 0.695074975490570, 0.696177005767822, 0.697277009487152,
	0.698375999927521, 0.699473023414612, 0.700568974018097, 0.701663017272949,
	0.702754974365234, 0.703845024108887, 0.704934000968933, 0.706021010875702,
	0.707107007503510, 0.708190977573395, 0.709272980690002, 0.710353016853333,
	0.711431980133057, 0.712508976459503, 0.713585019111633, 0.714658975601196,
	0.715731024742126, 0.716800987720490, 0.717869997024536, 0.718936979770660,
	0.720003008842468, 0.721065998077393, 0.722127974033356, 0.723188996315002,
	0.724246978759766, 0.725304007530212, 0.726359009742737, 0.727412998676300,
	0.728464007377625, 0.729514002799988, 0.730562984943390, 0.731608986854553,
	0.732653975486755, 0.733696997165680, 0.734739005565643, 0.735778987407684,
	0.736817002296448, 0.737852990627289, 0.738887012004852, 0.739920020103455,
	0.740951001644135, 0.741980016231537, 0.743008017539978, 0.744033992290497,
	0.745058000087738, 0.746079981327057, 0.747101008892059, 0.748118996620178,
	0.749135971069336, 0.750151991844177, 0.751164972782135, 0.752177000045776,
	0.753187000751495, 0.754194974899292, 0.755200982093811, 0.756205976009369,
	0.757209002971649, 0.758210003376007, 0.759208977222443, 0.760206997394562,
	0.761201977729797, 0.762196004390717, 0.763188004493713, 0.764178991317749,
	0.765166997909546, 0.766153991222382, 0.767139017581940, 0.768122017383575,
	0.769102990627289, 0.770083010196686, 0.771061003208160, 0.772036015987396,
	0.773010015487671, 0.773983001708984, 0.774953007698059, 0.775922000408173,
	0.776888012886047, 0.777853012084961, 0.778816998004913, 0.779778003692627,
	0.780736982822418, 0.781695008277893, 0.782651007175446, 0.783604979515076,
	0.784556984901428, 0.785507023334503, 0.786454975605011, 0.787401974201202,
	0.788345992565155, 0.789288997650146, 0.790229976177216, 0.791168987751007,
	0.792106986045837, 0.793042004108429, 0.793974995613098, 0.794906973838806,
	0.795836985111237, 0.796765029430389, 0.797690987586975, 0.798614978790283,
	0.799537003040314, 0.800458014011383, 0.801375985145569, 0.802293002605438,
	0.803207993507385, 0.804120004177094, 0.805031001567841, 0.805939972400665,
	0.806847989559174, 0.807753026485443, 0.808655977249146, 0.809557974338531,
	0.810456991195679, 0.811354994773865, 0.812250971794128, 0.813144028186798,
	0.814036011695862, 0.814926028251648, 0.815814018249512, 0.816700994968414,
	0.817584991455078, 0.818467020988464, 0.819347977638245, 0.820226013660431,
	0.821102023124695, 0.821977019309998, 0.822849988937378, 0.823720991611481,
	0.824589014053345, 0.825456023216248, 0.826321005821228, 0.827184021472931,
	0.828045010566711, 0.828903973102570, 0.829761028289795, 0.830615997314453,
	0.831470012664795, 0.832320988178253, 0.833169996738434, 0.834017992019653,
	0.834863007068634, 0.835705995559692, 0.836547970771790, 0.837387025356293,
	0.838225007057190, 0.839060008525848, 0.839893996715546, 0.840725004673004,
	0.841554999351501, 0.842383027076721, 0.843208014965057, 0.844031989574432,
	0.844853997230530, 0.845673024654388, 0.846490979194641, 0.847307026386261,
	0.848119974136353, 0.848932027816772, 0.849741995334625, 0.850549995899200,
	0.851355016231537, 0.852159023284912, 0.852961003780365, 0.853760004043579,
	0.854557991027832, 0.855354011058807, 0.856146991252899, 0.856939017772675,
	0.857729017734528, 0.858515977859497, 0.859301984310150, 0.860085010528564,
	0.860867023468018, 0.861645996570587, 0.862424015998840, 0.863198995590210,
	0.863973021507263, 0.864744007587433, 0.865513980388641, 0.866280972957611,
	0.867045998573303, 0.867808997631073, 0.868570983409882, 0.869329988956451,
	0.870087027549744, 0.870841979980469, 0.871595025062561, 0.872345983982086,
	0.873094975948334, 0.873842000961304, 0.874586999416351, 0.875329017639160,
	0.876070022583008, 0.876809000968933, 0.877544999122620, 0.878279983997345,
	0.879011988639832, 0.879742980003357, 0.880470991134644, 0.881196975708008,
	0.881920993328094, 0.882642984390259, 0.883363008499146, 0.884081006050110,
	0.884796977043152, 0.885510981082916, 0.886223018169403, 0.886932015419006,
	0.887639999389648, 0.888345003128052, 0.889047980308533, 0.889750003814697,
	0.890448987483978, 0.891146004199982, 0.891840994358063, 0.892534017562866,
	0.893224000930786, 0.893912971019745, 0.894599020481110, 0.895283997058868,
	0.895965993404388, 0.896646022796631, 0.897324979305267, 0.898001015186310,
	0.898674011230469, 0.899345993995667, 0.900016009807587, 0.900682985782623,
	0.901349008083344, 0.902011990547180, 0.902673006057739, 0.903331995010376,
	0.903989017009735, 0.904644012451172, 0.905296981334686, 0.905947029590607,
	0.906596004962921, 0.907242000102997, 0.907886028289795, 0.908527970314026,
	0.909168004989624, 0.909806013107300, 0.910440981388092, 0.911074995994568,
	0.911705970764160, 0.912334978580475, 0.912962019443512, 0.913586974143982,
	0.914210021495819, 0.914830029010773, 0.915449023246765, 0.916064977645874,
	0.916679024696350, 0.917290985584259, 0.917900979518890, 0.918507993221283,
	0.919113993644714, 0.919717013835907, 0.920318007469177, 0.920916974544525,
	0.921513974666595, 0.922109007835388, 0.922701001167297, 0.923291027545929,
	0.923879981040955, 0.924465000629425, 0.925049006938934, 0.925630986690521,
	0.926209986209869, 0.926787018775940, 0.927362978458405, 0.927935004234314,
	0.928506016731262, 0.929075002670288, 0.929641008377075, 0.930204987525940,
	0.930766999721527, 0.931326985359192, 0.931883990764618, 0.932439982891083,
	0.932992994785309, 0.933543980121613, 0.934092998504639, 0.934638977050781,
	0.935184001922607, 0.935725986957550, 0.936266005039215, 0.936802983283997,
	0.937339007854462, 0.937871992588043, 0.938404023647308, 0.938932001590729,
	0.939459025859833, 0.939984023571014, 0.940505981445312, 0.941025972366333,
	0.941543996334076, 0.942059993743896, 0.942573010921478, 0.943084001541138,
	0.943593978881836, 0.944100022315979, 0.944604992866516, 0.945106983184814,
	0.945607006549835, 0.946105003356934, 0.946600973606110, 0.947094023227692,
	0.947585999965668, 0.948074996471405, 0.948561012744904, 0.949046015739441,
	0.949527978897095, 0.950007975101471, 0.950486004352570, 0.950962007045746,
	0.951435029506683, 0.951906025409698, 0.952374994754791, 0.952841997146606,
	0.953306019306183, 0.953768014907837, 0.954227983951569, 0.954685986042023,
	0.955141007900238, 0.955594003200531, 0.956044971942902, 0.956493973731995,
	0.956939995288849, 0.957385003566742, 0.957826018333435, 0.958266019821167,
	0.958702981472015, 0.959138989448547, 0.959572017192841, 0.960002005100250,
	0.960430979728699, 0.960856974124908, 0.961279988288879, 0.961701989173889,
	0.962121009826660, 0.962538003921509, 0.962952971458435, 0.963365972042084,
	0.963775992393494, 0.964183986186981, 0.964590013027191, 0.964993000030518,
	0.965394020080566, 0.965793013572693, 0.966189980506897, 0.966584026813507,
	0.966975986957550, 0.967365980148315, 0.967754006385803, 0.968138992786407,
	0.968522012233734, 0.968903005123138, 0.969281017780304, 0.969657003879547,
	0.970031023025513, 0.970403015613556, 0.970772027969360, 0.971139013767242,
	0.971503973007202, 0.971866011619568, 0.972226977348328, 0.972584009170532,
	0.972940027713776, 0.973293006420135, 0.973644018173218, 0.973993003368378,
	0.974339008331299, 0.974684000015259, 0.975024998188019, 0.975364983081818,
	0.975701987743378, 0.976037025451660, 0.976369976997375, 0.976700007915497,
	0.977028012275696, 0.977353990077972, 0.977676987648010, 0.977998971939087,
	0.978317022323608, 0.978633999824524, 0.978947997093201, 0.979260027408600,
	0.979569971561432, 0.979876995086670, 0.980181992053986, 0.980485022068024,
	0.980785012245178, 0.981082975864410, 0.981378972530365, 0.981673002243042,
	0.981963992118835, 0.982253015041351, 0.982538998126984, 0.982824027538300,
	0.983105003833771, 0.983385026454926, 0.983662009239197, 0.983937025070190,
	0.984210014343262, 0.984480023384094, 0.984749019145966, 0.985014021396637,
	0.985278010368347, 0.985539019107819, 0.985798001289368, 0.986054003238678,
	0.986307978630066, 0.986559987068176, 0.986809015274048, 0.987056970596313,
	0.987300992012024, 0.987544000148773, 0.987784028053284, 0.988022029399872,
	0.988258004188538, 0.988490998744965, 0.988722026348114, 0.988950014114380,
	0.989176988601685, 0.989400029182434, 0.989621996879578, 0.989840984344482,
	0.990058004856110, 0.990272998809814, 0.990485012531281, 0.990694999694824,
	0.990903019905090, 0.991108000278473, 0.991311013698578, 0.991510987281799,
	0.991710007190704, 0.991905987262726, 0.992098987102509, 0.992290973663330,
	0.992479979991913, 0.992666006088257, 0.992850005626678, 0.993031978607178,
	0.993211984634399, 0.993389010429382, 0.993564009666443, 0.993736982345581,
	0.993906974792480, 0.994075000286102, 0.994239985942841, 0.994404017925262,
	0.994565010070801, 0.994723021984100, 0.994879007339478, 0.995033025741577,
	0.995185017585754, 0.995334029197693, 0.995481014251709, 0.995625019073486,
	0.995766997337341, 0.995907008647919, 0.996044993400574, 0.996179997920990,
	0.996312975883484, 0.996442973613739, 0.996571004390717, 0.996697008609772,
	0.996819972991943, 0.996940970420837, 0.997060000896454, 0.997175991535187,
	0.997290015220642, 0.997402012348175, 0.997511029243469, 0.997618019580841,
	0.997722983360291, 0.997825026512146, 0.997924983501434, 0.998022973537445,
	0.998117983341217, 0.998211026191711, 0.998301982879639, 0.998390018939972,
	0.998476028442383, 0.998558998107910, 0.998640000820160, 0.998718976974487,
	0.998794972896576, 0.998870015144348, 0.998941004276276, 0.999010980129242,
	0.999077975749969, 0.999141991138458, 0.999204993247986, 0.999265015125275,
	0.999321997165680, 0.999378025531769, 0.999431014060974, 0.999481022357941,
	0.999529004096985, 0.999575018882751, 0.999619007110596, 0.999660015106201,
	0.999698996543884, 0.999734997749329, 0.999768972396851, 0.999800980091095,
	0.999831020832062, 0.999858021736145, 0.999881982803345, 0.999904990196228,
	0.999925017356873, 0.999942004680634, 0.999957978725433, 0.999970972537994,
	0.999980986118317, 0.999988973140717, 0.999994993209839, 1.000000000000000
};

/* ---- lbcommon.c:320-388 lbCommonSin, lbCommonCos, lbCommonTan, verbatim -- */
// 0x800C7840
f32 lbCommonSin(f32 angle)
{
	u16 index = ((s32) (angle * 651.8986206F)) & 0xFFF;
    f32 sin;
    
    if (index & 0x400)
    {
        sin = dLBCommonSinLookup[0x3FF - (index & 0x3FF)];
    }
    else sin = dLBCommonSinLookup[index & 0x3FF];
    
    if (index & 0x800)
    {
        return -sin;
    }
    else return sin;
}

// 0x800C78B8
f32 lbCommonCos(f32 angle)
{
    u16 index = ((s32) ((angle + F_CST_DTOR32(90.0F)) * 651.8986206F)) & 0xFFF;
    f32 cos;
    
    if (index & 0x400)
    {
        cos = dLBCommonSinLookup[0x3FF - (index & 0x3FF)];
    }
    else cos = dLBCommonSinLookup[index & 0x3FF];
    
    if (index & 0x800)
    {
        return -cos;
    }
    else return cos;
}

// 0x800C793C
f32 lbCommonTan(f32 angle)
{
    u16 index = ((s32) (angle * 651.8986206F)) & 0xFFF;
    f32 sin, cos;
    
    if (index & 0x400)
    {
        sin = dLBCommonSinLookup[0x3FF - (index & 0x3FF)];
    }
    else sin = dLBCommonSinLookup[index & 0x3FF];
    
    if (index & 0x800)
    {
        sin = -sin;
    }
    index = (index + 0x400) & 0xFFF;
    
    if (index & 0x400)
    {
        cos = dLBCommonSinLookup[0x3FF - (index & 0x3FF)];
    }
    else cos = dLBCommonSinLookup[index & 0x3FF];
    
    if (index & 0x800)
    {
        cos = -cos;
    }
    return sin / cos;
}

/* lbcommon.c:390-408 lbCommonNormDist2D 0x800C7A00, verbatim: normalise a
 * vector's x/y in place and return its old length (0 if it was zero).
 * wpMainApplyGravityClampTVel uses it to clamp a weapon's air
 * velocity to its terminal speed. */
f32 lbCommonNormDist2D(Vec3f *vec)
{
    f32 magnitude;
    f32 factor;

    magnitude = sqrtf(SQUARE(vec->x) + SQUARE(vec->y));

    if (magnitude == 0.0F)
    {
        return 0.0F;
    }
    factor = 1.0F / magnitude;

    vec->x = vec->x * factor;
    vec->y = vec->y * factor;

    return magnitude;
}

/* lbcommon.c:410-413 lbCommonMag2D 0x800C7A84, verbatim: the length of a
 * vector's x/y (its z ignored). The damage collision
 * (mpCommonProcFighterDamage) measures a launched fighter's step against
 * the wall it hit through this. */
f32 lbCommonMag2D(Vec3f *vec)
{
    return sqrtf(SQUARE(vec->x) + SQUARE(vec->y));
}

/* lbcommon.c:416-423 lbCommonAdd2D 0x800C7AB8, verbatim: a += b in x/y.
 * The wall bounce (ftCommonWallDamageSetStatus) folds the
 * fighter's live air velocity onto its stacked knockback with this. */
Vec3f* lbCommonAdd2D(Vec3f *a, Vec3f *b)
{
    a->x = a->x + b->x;
    a->y = a->y + b->y;

    return a;
}

/* lbcommon.c:425-432 lbCommonScale2D 0x800C7AE0, verbatim: v *= factor in
 * x/y. The wall bounce keeps 0.8 of the reflected knockback. */
Vec3f* lbCommonScale2D(Vec3f *vec, f32 factor)
{
    vec->x = vec->x * factor;
    vec->y = vec->y * factor;

    return vec;
}

/* lbcommon.c:434-442 lbCommonReflect2D 0x800C7B08, verbatim: reflects a
 * across the wall whose (unit) normal is b -- a -= 2(a.b)b in x/y. This is
 * what turns a launch into the wall into a bounce back off it. */
Vec3f* lbCommonReflect2D(Vec3f *a, Vec3f *b)
{
    f32 negative_two_dot_product = (b->x * a->x + b->y * a->y) * -2.0F;

    a->x = a->x + b->x * negative_two_dot_product;
    a->y = a->y + b->y * negative_two_dot_product;

    return a;
}

/* lbcommon.c:454-461 lbCommonSim2D 0x800C7C0C, verbatim: the 2D cosine
 * similarity a.b/(|a|+|b|) -- weapons use its sign against a wall's angle
 * to decide whether a launch is heading into the wall before bouncing it
 * (wpMapCheckAllRebound). Pure math, no N64 service. */
f32 lbCommonSim2D(Vec3f *a, Vec3f *b)
{
    f32 magnitude_a = sqrtf(SQUARE(a->x) + SQUARE(a->y));
    f32 magnitude_b = sqrtf(SQUARE(b->x) + SQUARE(b->y));

    return (a->x * b->x + a->y * b->y) / (magnitude_b + magnitude_a);
}

/* lbcommon.c:462-486 lbCommonCheckAdjustSim2D 0x800C7C98, verbatim: the
 * grazing test Fire Fox's aerial ProcMap runs against every surface it
 * touches. `a` is the fighter's air velocity, `b` the
 * surface's angle, `angle` the widest miss that still counts as a graze
 * (FTFOX_FIREFOX_BOUND_ANGLE, 20 degrees). If the two point against each
 * other but by less than 90+angle, the velocity is rotated onto the
 * surface -- it keeps its magnitude and slides along, sign picked by the
 * 2D cross -- and TRUE says the fighter grazed rather than hit. Pure
 * math, no N64 service. */
sb32 lbCommonCheckAdjustSim2D(Vec3f *a, Vec3f *b, f32 angle)
{
    f32 similarity;
    f32 orientation;
    f32 magnitude;

    similarity = lbCommonSim2D(b, a);

    if (similarity <= 0.0F)
    {
        if (similarity >= cosf(angle + F_CST_DTOR32(90.0F)))
        {
            orientation = b->x * a->y - b->y * a->x;

            orientation = (orientation < 0.0F) ? -1.0F : 1.0F;

            magnitude = lbCommonMag2D(a) * orientation;

            a->x = -b->y * magnitude;
            a->y = b->x * magnitude;

            return TRUE;
        }
    }
    return FALSE;
}

/* lb/lbcommon.c:444-451 lbCommonSim3D 0x800C7B58, verbatim: a similarity
 * of two vectors -- their dot product over the SUM of their lengths, which
 * the game's one caller in VS, matrix kind 0x49 (src/dc/objdisplay.c),
 * compares against 0.999. */
// 0x800C7B58
f32 lbCommonSim3D(Vec3f *a, Vec3f *b)
{
	f32 magnitude_a = sqrtf(SQUARE(a->x) + SQUARE(a->y) + SQUARE(a->z));
	f32 magnitude_b = sqrtf(SQUARE(b->x) + SQUARE(b->y) + SQUARE(b->z));

	return (a->x * b->x + a->y * b->y + a->z * b->z) / (magnitude_b + magnitude_a);
}

/* lbcommon.c:3029-3034 lbCommonCross3D 0x800CD5AC, verbatim: the 3D cross
 * product out = a x b. Only wpMainReflectorRotateWeaponModel (Master
 * Hand's reflected bullets) needs it so far; it lives here because
 * it is lb code. */
void lbCommonCross3D(Vec3f *a, Vec3f *b, Vec3f *out)
{
    out->x = a->y * b->z - a->z * b->y;
    out->y = a->z * b->x - a->x * b->z;
    out->z = a->x * b->y - a->y * b->x;
}

/* ---- lbcommon.c:725-748 lbCommonMakePositionFGM 0x800C8654, verbatim.
 * The pair under it allocates the voice detached, so this can set its
 * balance before it makes a sound, and commits it (src/dc/syaudio.c
 * over src/dc/fgm.c). `pos` is a world x, so a sound made 8000 units
 * off-centre is hard left or hard right and one at the origin is 64. */
alSoundEffect* lbCommonMakePositionFGM(u16 fgm, f32 pos)
{
    alSoundEffect *snd = func_80026A10_27610(fgm);
    
    if (snd != NULL)
    {
        s32 balance = ((pos / 8000.0F) * 60.0F);
        
        if (balance > 60)
        {
            balance = 60;
        }
        if (balance < -60)
        {
            balance = -60;
        }
        balance = 64 - balance;
        
        snd->balance = balance;

        func_800267F4_273F4(snd);
    }
    return snd;
}

/* ======================================================================
 * lb/lbcommon.c:285-300 and 2218-3007: the sprite renderer, ported.
 * See lbcommon.h for the shape of the port.
 *
 * The game draws a sprite as texture rectangles, one per Bitmap strip,
 * after setting the RDP up for it: cycle type, render mode, texture
 * filter, TLUT mode, primitive and environment colours and the colour
 * combiner, each one a gDP* command emitted only when it differs from
 * what the previous sprite left (that is what the sLBCommonExtern*
 * statics track). The port keeps every one of those functions and their
 * order, and gives the gDP* macros a new expansion: each writes the RDP
 * register it names into sLBCommonRDP, a model of the eight registers
 * the sprite code touches. The rectangle then reads the model and
 * submits one PVR quad carrying the texture and the state together --
 * which is what a PVR polygon header is. The arithmetic between the two,
 * position and scissor and texel stepping, is the decomp's own text.
 *
 * Two of the RDP's behaviours have no PVR equivalent and DIVERGE:
 *   - G_RM_OPA_SURF with G_AC_THRESHOLD (lbCommonStartSprite sets the
 *     threshold to 8/255) writes a texel either fully or not at all. The
 *     PVR's translucent list blends by alpha instead, so a sprite the
 *     game cuts out hard has soft edges here. Every sprite goes to the
 *     translucent list because it has to draw over the opaque list's
 *     geometry, which the PVR resolves by depth: sprites are given a
 *     depth above anything the 3D cameras produce, stepped per
 *     rectangle so later ones land on top (the RDP has no depth for
 *     these; it draws them in order).
 *   - G_CYC_COPY (SP_FASTCOPY) copies texels 1:1 with no filtering. The
 *     PVR draws the same rectangle with point sampling.
 * A third is a matter of degree: the RDP's bilinear filter and the
 * PVR's take the same texel centre convention, so the PVR's sample at
 * pixel (x + 1/4, x + 3/4) of the doubled framebuffer straddles the
 * RDP's sample at x; the sprite is drawn at 2x, not resampled.
 * ==================================================================== */
#include <string.h>
#include <sys/obj.h>
#include <sys/objman.h>          /* dGCTranslateDefault and the other two, lbCommonInitDObj */
#include <sys/objanim.h>         /* gcAddDObjAnimJoint, lbCommonAddFighterPartsFigatree */
#include <sys/interp.h>          /* syInterpCubic, lbCommonPlayTranslateScaledDObjAnim */
#include <ft/fttypes.h>          /* FTParts, the same */
#include <sys/debug.h>
#include <sys/video.h>
#include <sys/taskman.h>         /* dSYTaskmanFrameCount, the depth counter's frame */
#include <sc/scene.h>            /* scManagerRunPrintGObjStatus */
#include <config.h>              /* GS_SCREEN_WIDTH_DEFAULT */
#include "sprite.h"
#include "objpvr.h"
#include "perf.h"
#include "objmodel.h"

/* lb/lbcommon.c:312: the scale a fighter joint's matrix kind 0x4B passes
 * from a joint to its children while the fighter's motion runs with
 * is_use_animlocks (src/dc/objdisplay.c gcDObjLocalMatrix). */
Vec3f gLBCommonScale;

#ifndef SSB_NO_DRAW
#include <dc/pvr.h>
#endif

/* ---- lbcommon.c:285-300, verbatim -------------------------------- */
// 0x800D62B0
static u16 sLBCommonExternSpriteAttr;

// 0x800D62B2
static u16 sLBCommonExternBitmapFmt;

// 0x800D62B4
static void *sLBCommonPrevBitmapBuf;

// 0x800D62B8
static void *sLBCommonPrevSpriteLUT;

// 0x800D62BC
static s32 sLBCommonScissorXMax;

// 0x800D62C0
static s32 sLBCommonScissorYMax;

// 0x800D62C4
static s32 sLBCommonScissorXMin;

// 0x800D62C8
static s32 sLBCommonScissorYMin;

/* ---- the port's RDP -------------------------------------------------
 *
 * The registers the sprite code sets, and the combiner reduced to the
 * four programs it uses. `dirty` says a register changed since the last
 * quad, so the next rectangle carries a new polygon header even if it
 * draws the same texture. */
typedef struct LBCommonRDP
{
    u32 cycle;                  /* G_CYC_1CYCLE or G_CYC_COPY */
    u32 render;                 /* G_RM_OPA_SURF, G_RM_XLU_SURF, G_RM_CLD_SURF */
    u32 filter;                 /* G_TF_BILERP or G_TF_AVERAGE */
    u32 tlut;                   /* G_TT_NONE or G_TT_RGBA16 */
    u8 prim[4];
    u8 env[4];
    s32 combine;                /* nLBCommonCombine* */
    u32 wrap_s, wrap_t;         /* the tile's cms/masks and cmt/maskt,
                                   reduced to "repeats past the edge" */
    sb32 dirty;
} LBCommonRDP;

static LBCommonRDP sLBCommonRDP;
static s32 sLBCommonSpriteRectCount;   /* rectangles this frame, for depth */
static u32 sLBCommonSpriteFrame;       /* the frame the count is for */
static s32 sLBCommonSpriteBackdropCount; /* the backdrop band's own count */
static f32 sLBCommonStack, sLBCommonStackUnder; /* lbCommonBackdropStackTake's
                                                * cursors, 0 = the floor */
static sb32 sLBCommonSpriteBackdrop;   /* lbCommonDrawSpriteBackdrop is
                                        * capturing: depths come from the
                                        * backdrop band (lbcommon.h) */

#ifdef SSB_NO_DRAW
LBCommonSpriteQuad gLBCommonSpriteQuadLog[LB_SPRITE_QUAD_LOG_MAX];
s32 gLBCommonSpriteQuadLogCount;
#endif

static void lbCommonRDPSet(u32 *reg, u32 val)
{
    if (*reg != val)
    {
        *reg = val;
        sLBCommonRDP.dirty = TRUE;
    }
}

static void lbCommonRDPSetColor(u8 *reg, u32 r, u32 g, u32 b, u32 a)
{
    if ((reg[0] != r) || (reg[1] != g) || (reg[2] != b) || (reg[3] != a))
    {
        reg[0] = r;
        reg[1] = g;
        reg[2] = b;
        reg[3] = a;
        sLBCommonRDP.dirty = TRUE;
    }
}

/* The colour combiner, classified. The two-cycle programs the sprite
 * code sets are the same in both cycles, so one cycle's eight inputs
 * are enough to tell them apart:
 *   RGB = (a - b) * c + d,  A = (Aa - Ab) * Ac + Ad. */
static void lbCommonRDPSetCombineLERP(u32 a, u32 b, u32 c, u32 d,
                                      u32 Aa, u32 Ab, u32 Ac, u32 Ad)
{
    s32 combine;

    if ((a == G_CCMUX_PRIMITIVE) && (b == G_CCMUX_ENVIRONMENT) &&
        (c == G_CCMUX_TEXEL0) && (d == G_CCMUX_ENVIRONMENT))
    {
        combine = nLBCommonCombineIAPrimEnv;        /* lerp(ENV, PRIM, I) */
    }
    else if ((a == G_CCMUX_TEXEL0) && (b == G_CCMUX_PRIMITIVE) &&
             (c == G_CCMUX_ENVIRONMENT) && (d == G_CCMUX_PRIMITIVE))
    {
        combine = nLBCommonCombineTexPrimEnv;       /* lerp(PRIM, TEX, ENV) */
    }
    else if ((a == G_CCMUX_NOISE) && (b == G_CCMUX_TEXEL0) &&
             (c == G_CCMUX_PRIMITIVE) && (d == G_CCMUX_TEXEL0))
    {
        /* (NOISE - TEX) * PRIM + TEX, the static over a locked
         * character's portrait (mnPlayersVSPortraitProcDisplay, PRIM
         * 0x30). DIVERGES: the PVR has no noise input; the texel stands */
        combine = nLBCommonCombineDecal;
    }
    else if ((a == G_CCMUX_0) && (b == G_CCMUX_0) && (c == G_CCMUX_0) &&
             (d == G_CCMUX_TEXEL0) && (Aa == G_ACMUX_TEXEL0) &&
             (Ac == G_ACMUX_PRIMITIVE))
    {
        combine = nLBCommonCombineTexPrimAlpha;     /* TEX, A * PRIM.a */
    }
    else if ((a == G_CCMUX_TEXEL0) && (b == G_CCMUX_0) &&
             (c == G_CCMUX_PRIMITIVE) && (d == G_CCMUX_0))
    {
        combine = nLBCommonCombineTexPrim;          /* G_CC_MODULATEI_PRIM */
    }
    else if ((a == G_CCMUX_0) && (b == G_CCMUX_0) && (c == G_CCMUX_0) &&
             (d == G_CCMUX_PRIMITIVE))
    {
        /* RGB = PRIM; the alpha side says whether PRIM's alpha scales
         * the texel's (mnTitleLogoProcDisplay) or the texel's stands */
        combine = ((Aa == G_ACMUX_TEXEL0) && (Ac == G_ACMUX_PRIMITIVE))
                  ? nLBCommonCombineIPrimAlpha : nLBCommonCombineIPrim;
    }
    else
    {
        static sb32 warned = FALSE;

        if (warned == FALSE)
        {
            warned = TRUE;
            syDebugPrintf("lbCommon: sprite combiner %lu %lu %lu %lu / "
                          "%lu %lu %lu %lu not modelled; drawing decal\n",
                          (unsigned long)a, (unsigned long)b,
                          (unsigned long)c, (unsigned long)d,
                          (unsigned long)Aa, (unsigned long)Ab,
                          (unsigned long)Ac, (unsigned long)Ad);
        }
        combine = nLBCommonCombineDecal;
    }
    lbCommonRDPSet((u32 *)&sLBCommonRDP.combine, (u32)combine);
}

void lbCommonSpriteSetCombine(s32 combine)
{
    lbCommonRDPSet((u32 *)&sLBCommonRDP.combine, (u32)combine);
}

void lbCommonSpriteSetPrimColor(u32 r, u32 g, u32 b, u32 a)
{
    lbCommonRDPSetColor(sLBCommonRDP.prim, r, g, b, a);
}

void lbCommonSpriteSetEnvColor(u32 r, u32 g, u32 b, u32 a)
{
    lbCommonRDPSetColor(sLBCommonRDP.env, r, g, b, a);
}

/* The GBI, re-aimed at the model. PR/gbi.h defined these to write RSP
 * command words; from here to the end of the file they write
 * sLBCommonRDP, and the `dl++` each is handed is unused. The names are
 * kept so the functions below stay the decomp's text. */
#undef gDPPipeSync
#undef gDPLoadSync
#undef gDPSetCycleType
#undef gDPSetRenderMode
#undef gDPSetTextureFilter
#undef gDPSetTextureLUT
#undef gDPSetPrimColor
#undef gDPSetEnvColor
#undef gDPSetCombineMode
#undef gDPSetCombineLERP
#undef gDPLoadTLUT
#undef gDPSetBlendColor
#undef gDPSetAlphaCompare
#undef gDPSetTexturePersp
#undef gDPSetTextureConvert
#undef gDPSetTextureDetail
#undef gDPSetTextureLOD
#define gDPPipeSync(pkt)                        ((void)0)
#define gDPLoadSync(pkt)                        ((void)0)
#define gDPSetCycleType(pkt, type)              lbCommonRDPSet(&sLBCommonRDP.cycle, (u32)(type))
#define gDPSetRenderMode(pkt, c0, c1)           lbCommonRDPSet(&sLBCommonRDP.render, (u32)(c0))
#define gDPSetTextureFilter(pkt, type)          lbCommonRDPSet(&sLBCommonRDP.filter, (u32)(type))
#define gDPSetTextureLUT(pkt, type)             lbCommonRDPSet(&sLBCommonRDP.tlut, (u32)(type))
#define gDPSetPrimColor(pkt, m, l, r, g, b, a)  lbCommonRDPSetColor(sLBCommonRDP.prim, (r), (g), (b), (a))
#define gDPSetEnvColor(pkt, r, g, b, a)         lbCommonRDPSetColor(sLBCommonRDP.env, (r), (g), (b), (a))
#define gDPSetCombineMode(pkt, a, b)            lbCommonRDPSet((u32 *)&sLBCommonRDP.combine, nLBCommonCombineDecal)
#define gDPSetCombineLERP(pkt, a0, b0, c0, d0, Aa0, Ab0, Ac0, Ad0, a1, b1, c1, d1, Aa1, Ab1, Ac1, Ad1) \
    lbCommonRDPSetCombineLERP(G_CCMUX_##a0, G_CCMUX_##b0, G_CCMUX_##c0, G_CCMUX_##d0, \
                              G_ACMUX_##Aa0, G_ACMUX_##Ab0, G_ACMUX_##Ac0, G_ACMUX_##Ad0)
#define gDPLoadTLUT(pkt, count, offset, dram)   ((void)0)
#define gDPSetBlendColor(pkt, r, g, b, a)       ((void)0)
#define gDPSetAlphaCompare(pkt, type)           ((void)0)
#define gDPSetTexturePersp(pkt, type)           ((void)0)
#define gDPSetTextureConvert(pkt, type)         ((void)0)
#define gDPSetTextureDetail(pkt, type)          ((void)0)
#define gDPSetTextureLOD(pkt, type)             ((void)0)

/* ---- the rectangle, as a quad --------------------------------------- */

/* gSPTextureRectangle's arguments to a PVR quad. Screen coordinates are
 * 10.2 fixed point in the game's 320x240; s and t are 10.5, the texel
 * steps 5.10. In copy mode the RDP's lower-right edge is inclusive and
 * its s step is given as four texels per pixel (it moves 64 bits a
 * cycle), which is 1:1. */
void lbCommonSpriteQuadOf(const LBCommonSpriteRect *r, const DCSpriteTex *tex,
                          LBCommonSpriteQuad *q)
{
    f32 px0 = r->rxh * 0.25F;
    f32 py0 = r->ryh * 0.25F;
    f32 px1 = r->rxl * 0.25F;
    f32 py1 = r->ryl * 0.25F;
    f32 dsdx = r->sx * (1.0F / 1024.0F);
    f32 dtdy = r->sy * (1.0F / 1024.0F);
    f32 s0, t0, s1, t1;

    if (r->copy)
    {
        dsdx *= 0.25F;
        px1 += 1.0F;
        py1 += 1.0F;
    }
    s0 = r->rs * (1.0F / 32.0F);
    t0 = (f32)r->t0 + r->rt * (1.0F / 32.0F);
    s1 = s0 + (px1 - px0) * dsdx;
    t1 = t0 + (py1 - py0) * dtdy;

    q->x0 = px0 * LB_SPRITE_SCREEN_SCALE;
    q->y0 = py0 * LB_SPRITE_SCREEN_SCALE;
    q->x1 = px1 * LB_SPRITE_SCREEN_SCALE;
    q->y1 = py1 * LB_SPRITE_SCREEN_SCALE;
    q->u0 = s0 / (f32)tex->texw;
    q->v0 = t0 / (f32)tex->texh;
    q->u1 = s1 / (f32)tex->texw;
    q->v1 = t1 / (f32)tex->texh;
}

/* The vertex colours the modelled combiner reduces to, on a texture that
 * carries intensity in RGB and alpha in A (sprite.h):
 *   PVR: colour = tex * base + offset, alpha = tex.a * base.a. */
static void lbCommonSpriteColorsOf(u32 *argb, u32 *oargb)
{
    const u8 *p = sLBCommonRDP.prim;
    const u8 *e = sLBCommonRDP.env;

    switch (sLBCommonRDP.combine)
    {
    case nLBCommonCombineIPrim:
        *argb = 0xFF000000 | ((u32)p[0] << 16) | ((u32)p[1] << 8) | p[2];
        *oargb = 0;
        break;

    case nLBCommonCombineIPrimAlpha:
        *argb = ((u32)p[3] << 24) | ((u32)p[0] << 16) | ((u32)p[1] << 8) | p[2];
        *oargb = 0;
        break;

    case nLBCommonCombineTexPrim:
        *argb = ((u32)p[3] << 24) | ((u32)p[0] << 16) | ((u32)p[1] << 8) | p[2];
        *oargb = 0;
        break;

    case nLBCommonCombineTexPrimAlpha:
        /* the texel's own colour, its alpha scaled by PRIM's */
        *argb = ((u32)p[3] << 24) | 0x00FFFFFF;
        *oargb = 0;
        break;

    case nLBCommonCombineIAPrimEnv:
        /* lerp(ENV, PRIM, I) = I * (PRIM - ENV) + ENV. The PVR's base
         * colour cannot go negative, so an ENV brighter than PRIM in a
         * channel clamps -- no sprite in the game asks for one. */
        {
            u32 r = (p[0] > e[0]) ? p[0] - e[0] : 0;
            u32 g = (p[1] > e[1]) ? p[1] - e[1] : 0;
            u32 b = (p[2] > e[2]) ? p[2] - e[2] : 0;

            *argb = ((u32)p[3] << 24) | (r << 16) | (g << 8) | b;
            *oargb = ((u32)e[0] << 16) | ((u32)e[1] << 8) | e[2];
        }
        break;

    case nLBCommonCombineTexPrimEnv:
        /* lerp(PRIM, TEX, ENV) = TEX * ENV + PRIM * (1 - ENV): the
         * texel scaled by ENV with PRIM's share as the offset */
        {
            u32 r = (p[0] * (255 - e[0])) / 255;
            u32 g = (p[1] * (255 - e[1])) / 255;
            u32 b = (p[2] * (255 - e[2])) / 255;

            *argb = 0xFF000000 | ((u32)e[0] << 16) | ((u32)e[1] << 8) | e[2];
            *oargb = (r << 16) | (g << 8) | b;
        }
        break;

    default:
        *argb = 0xFFFFFFFF;
        *oargb = 0;
        break;
    }
}

/* The polygon header: this texture with the modelled RDP state. */
static void lbCommonSpriteSubmitHeader(const DCSpriteTex *tex)
{
#ifndef SSB_NO_DRAW
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    int filter = (sLBCommonRDP.cycle == G_CYC_COPY) ? PVR_FILTER_NONE
                                                    : PVR_FILTER_BILINEAR;
    int fmt = (tex->fmt == nDCSpriteTexFmtARGB1555) ? PVR_TXRFMT_ARGB1555
            : (tex->fmt == nDCSpriteTexFmtRGB565Stride)
                  ? (PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED |
                     PVR_TXRFMT_X32_STRIDE)
                  : PVR_TXRFMT_ARGB4444;

    pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, fmt, tex->texw, tex->texh,
                     tex->txr, filter);
    cxt.gen.specular = PVR_SPECULAR_ENABLE;
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.txr.env = PVR_TXRENV_MODULATEALPHA;
    /* the tile's wrap: a sprite tiled past its width (SObj.lrs > the
     * sprite's, with masks naming the tile's power of two) repeats on
     * the PVR the way it repeats in TMEM, because the exporter stores
     * a sprite's strips at the tile's width (sprite.h) */
    cxt.txr.uv_clamp = sLBCommonRDP.wrap_s
                           ? (sLBCommonRDP.wrap_t ? PVR_UVCLAMP_NONE : PVR_UVCLAMP_V)
                           : (sLBCommonRDP.wrap_t ? PVR_UVCLAMP_U : PVR_UVCLAMP_UV);
    cxt.blend.src = PVR_BLEND_SRCALPHA;
    cxt.blend.dst = PVR_BLEND_INVSRCALPHA;
    cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    cxt.depth.write = PVR_DEPTHWRITE_DISABLE;
    DBPERF_COMPILE();
    pvr_poly_compile(&hdr, &cxt);
    pvr_prim(&hdr, sizeof(hdr));
#else
    (void)tex;
#endif
    sLBCommonRDP.dirty = FALSE;
}

#if !defined(SSB_NO_DRAW) && defined(DB_SPRITE_DEPTH_BUDGET)
#include <kos.h>          /* dbglog, DBG_WARNING */
#endif

/* The depth the next quad draws at. The RDP drew rectangles in the order
 * it was given them, each over the last; the PVR sorts the translucent
 * list by depth, so every quad takes a depth a step past the one before.
 * The counter runs for the frame, not the pass: a scene with two sprite
 * cameras (mn/mncommon/mnmodeselect.c's decals under its labels) draws
 * the second camera's first rectangle over the first's only if its depth
 * is past everything already drawn, and a fill quad between passes
 * (lb/lbfade.c) the same. */
static void lbCommonSpriteDepthFrame(void)
{
    if (sLBCommonSpriteFrame != dSYTaskmanFrameCount)
    {
        sLBCommonSpriteFrame = dSYTaskmanFrameCount;
        sLBCommonSpriteRectCount = 0;
        sLBCommonSpriteBackdropCount = 0;
        sLBCommonStack = sLBCommonStackUnder = 0.0F;
    }
}

f32 lbCommonSpriteNextDepth(void)
{
    lbCommonSpriteDepthFrame();

    if (sLBCommonSpriteBackdrop)
    {
        /* a backdrop camera's capture: its own band and its own count,
         * so the front band's margin (below) is not spent on it */
        return LB_SPRITE_Z_BACKDROP_BASE +
               (f32)sLBCommonSpriteBackdropCount++ * LB_SPRITE_Z_BACKDROP_STEP;
    }
#if !defined(SSB_NO_DRAW) && defined(DB_SPRITE_DEPTH_BUDGET)
    /* -DDB_SPRITE_DEPTH_BUDGET, off by default: LB_SPRITE_Z_BASE's
     * comment assumes a frame never asks for enough of these to erode
     * its margin over real 3D content. 192 is 3/4 of the 1/256 step's
     * own room before that margin is gone -- past it the frame is
     * already spending more of it than the constant was chosen
     * against, worth knowing about before it becomes a visible tie. */
    if (sLBCommonSpriteRectCount == 192)
        dbglog(DBG_WARNING, "lbcommon: %u sprite depths this frame, "
               "past 3/4 of LB_SPRITE_Z_BASE's margin\n",
               (unsigned)sLBCommonSpriteRectCount);
#endif
    return LB_SPRITE_Z_BASE + (f32)sLBCommonSpriteRectCount++ * LB_SPRITE_Z_STEP;
}

/* See lbcommon.h: the backdrop band's next depth, asked for outside a
 * backdrop camera's capture. */
f32 lbCommonSpriteNextBackdropDepth(void)
{
    sb32 was = sLBCommonSpriteBackdrop;
    f32 depth;

    sLBCommonSpriteBackdrop = TRUE;
    depth = lbCommonSpriteNextDepth();
    sLBCommonSpriteBackdrop = was;

    return depth;
}

/* See lbcommon.h. The cursors move by the ratio and a hair more, so two
 * models' ranges never touch. */
f32 lbCommonBackdropStackTake(sb32 under, f32 ratio)
{
    f32 lo = under ? LB_Z_STACK_UNDER_LO : LB_Z_STACK_LO;
    f32 hi = under ? LB_Z_STACK_UNDER_HI : LB_Z_STACK_HI;
    f32 *cur = under ? &sLBCommonStackUnder : &sLBCommonStack;
    f32 base;

    lbCommonSpriteDepthFrame();
    if (*cur < lo)
    {
        *cur = lo;
    }
    if (ratio < 1.0F)
    {
        ratio = 1.0F;
    }
    base = *cur;
    if (base * ratio > hi)
    {
#if !defined(SSB_NO_DRAW) && defined(DB_SPRITE_DEPTH_BUDGET)
        dbglog(DBG_WARNING, "lbcommon: backdrop stack %d full (ratio %f)\n",
               (int)under, (double)ratio);
#endif
        base = hi / ratio;
    }
    *cur = base * ratio * 1.001F;

    return base;
}

static void lbCommonSpriteSubmitRect(const LBCommonSpriteRect *r,
                                     const DCSpriteTex *tex)
{
    LBCommonSpriteQuad q;
    u32 argb, oargb;

#ifndef SSB_NO_DRAW
    /* Translucent pass only (and see lbCommonSpriteFillRect), whoever captured the caller. A sprite
     * camera already returns early outside that pass, but a display
     * proc on a 3D camera's DL link (func_80017EC0) runs in all three,
     * and a TR-list header sent while the opaque list is open spoils
     * the whole frame: the Sound Test and How to Play drew nothing at
     * all. */
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
#endif

    lbCommonSpriteQuadOf(r, tex, &q);
    lbCommonSpriteColorsOf(&argb, &oargb);
    q.z = lbCommonSpriteNextDepth();
    q.argb = argb;
    q.oargb = oargb;

#ifndef SSB_NO_DRAW
    {
        pvr_vertex_t v;
        int i;

        v.oargb = oargb;
        v.argb = argb;
        v.z = q.z;
        for (i = 0; i < 4; i++)
        {
            v.flags = (i == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v.x = (i & 2) ? q.x1 : q.x0;
            v.y = (i & 1) ? q.y0 : q.y1;
            v.u = (i & 2) ? q.u1 : q.u0;
            v.v = (i & 1) ? q.v0 : q.v1;
            pvr_prim(&v, sizeof(v));
        }
    }
#else
    if (gLBCommonSpriteQuadLogCount < LB_SPRITE_QUAD_LOG_MAX)
    {
        q.rect = *r;
        gLBCommonSpriteQuadLog[gLBCommonSpriteQuadLogCount] = q;
    }
    gLBCommonSpriteQuadLogCount++;
#endif
}

/* ---- lbcommon.c:2218-2497 lbCommonDrawSObjBitmap 0x800CB7D4 -----------
 * Verbatim through the scissor arithmetic. The switch on bmsiz -- four
 * arms of gDPSetTextureImage / gDPSetTile / gDPLoadBlock / gDPSetTile /
 * gDPSetTileSize, one per texel size, each loading the strip into TMEM
 * -- is one call: the strip's texture is in VRAM, and what the PVR takes
 * with it is the state the gDP* calls above have been writing. The
 * `bitmap->buf != sLBCommonPrevBitmapBuf` test is the decomp's and does
 * the same job (skip the reload when the strip is already bound); the
 * port adds "or the state changed" because the PVR carries the state in
 * the same header. The rectangle is the quad. */
void lbCommonDrawSObjBitmap
(
    Gfx **dls,
    SObj *sobj,
    Sprite *sprite,
    Bitmap *bitmap,
    s32 x, s32 y,
    s32 xx, s32 yy,
    s32 fs, s32 ft,
    s32 sx, s32 sy
)
{
    s32 rs, rt;
    s32 rxh, ryh;
    s32 rxl, ryl;
    s32 tex_width, tex_height;
    LBCommonSpriteRect rect;
    const DCSpriteTex *tex;

    (void)dls;

    /* which texture: a CI sprite's is the palette its LUT names --
     * sprite.h on why a palette is a texture here -- and any other's is
     * the strip's. The game's gDPLoadTLUT of sprite->LUT above is the
     * same choice made in TMEM. */
    tex = (sprite->bmfmt == G_IM_FMT_CI && sprite->LUT != NULL)
              ? (const DCSpriteTex *)sprite->LUT
              : (const DCSpriteTex *)bitmap->buf;

    if (bitmap->buf == NULL)
    {
        while (TRUE)
        {
            syDebugPrintf("drawBitMap: no bitmap data!\n");
            scManagerRunPrintGObjStatus();
        }
    }
    if (yy >= sLBCommonScissorYMin)
    {
        if (sprite->attr & SP_FASTCOPY)
        {
            yy--;
        }
        if (sprite->attr & SP_TEXSHIFT)
        {
            fs += 16;
            ft += 16;
        }
        tex_width = bitmap->width;
        tex_height = bitmap->actualHeight;
        (void)tex_width;
        (void)tex_height;

        if (x < sLBCommonScissorXMin)
        {
            rxh = sLBCommonScissorXMin * 4;
            rs = fs + (((sLBCommonScissorXMin - x) * sx) >> 5);
        }
        else
        {
            rxh = x * 4;
            rs = fs;
        }
        if (y < sLBCommonScissorYMin)
        {
            ryh = sLBCommonScissorYMin * 4;
            rt = ft + (((sLBCommonScissorYMin - y) * sy) >> 5);
        }
        else
        {
            ryh = y * 4;
            rt = ft;
        }
        if (xx >= sLBCommonScissorXMax)
        {
            rxl = sLBCommonScissorXMax * 4;
        }
        else rxl = xx * 4;

        if (yy >= sLBCommonScissorYMax)
        {
            ryl = sLBCommonScissorYMax * 4;
        }
        else ryl = yy * 4;

        /* the gDPSetTile in the switch this stands for carries the
         * SObj's cms/masks and cmt/maskt: a mask of 0 clamps, and a
         * wrap mode with a mask repeats the tile (the mirror mode no
         * sprite in the game asks for would need a mirrored texture
         * and is drawn as a repeat) */
        lbCommonRDPSet(&sLBCommonRDP.wrap_s,
                       (sobj->masks != 0 && (sobj->cms & G_TX_CLAMP) == 0) ? 1 : 0);
        lbCommonRDPSet(&sLBCommonRDP.wrap_t,
                       (sobj->maskt != 0 && (sobj->cmt & G_TX_CLAMP) == 0) ? 1 : 0);

        if ((tex != sLBCommonPrevBitmapBuf) || sLBCommonRDP.dirty)
        {
            lbCommonSpriteSubmitHeader(tex);
            sLBCommonPrevBitmapBuf = (void *)tex;
        }
        rect.rxh = rxh;
        rect.ryh = ryh;
        rect.rxl = rxl;
        rect.ryl = ryl;
        rect.rs = rs;
        rect.rt = rt;
        rect.sx = sx;
        rect.sy = sy;
        rect.t0 = bitmap->t;
        rect.copy = (sprite->attr & SP_FASTCOPY) ? TRUE : FALSE;
        lbCommonSpriteSubmitRect(&rect, tex);
    }
}

/* ---- lbcommon.c:2499-2644 lbCommonPrepSObjAttr 0x800CC118, verbatim.
 * The gDP* calls write the port's RDP model (above). The second
 * `else if (sprite->attr & SP_TRANSPARENT)` is the decomp's: a cloud
 * sprite drawn first after lbCommonClearExternSpriteParams gets the
 * opaque render mode. -------------------------------------------------- */
void lbCommonPrepSObjAttr(Gfx **dls, SObj *sobj)
{
    Gfx *dl = NULL;
    Sprite *sprite = &sobj->sprite;

    (void)dls;
    (void)dl;

    if (sLBCommonExternSpriteAttr & SP_ARGUMENT)
    {
        gDPPipeSync(dl++);

        if (sprite->attr & SP_FASTCOPY)
        {
            gDPSetCycleType(dl++, G_CYC_COPY);
        }
        else gDPSetCycleType(dl++, G_CYC_1CYCLE);

        if (sprite->attr & SP_TRANSPARENT)
        {
            gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        }
        else if (sprite->attr & SP_TRANSPARENT)
        {
            gDPSetRenderMode(dl++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        }
        else gDPSetRenderMode(dl++, G_RM_OPA_SURF, G_RM_OPA_SURF2);

        if (sprite->attr & SP_TEXSHIFT)
        {
            gDPSetTextureFilter(dl++, G_TF_AVERAGE);
        }
        else gDPSetTextureFilter(dl++, G_TF_BILERP);

        if (sprite->bmfmt != G_IM_FMT_CI)
        {
            gDPSetTextureLUT(dl++, G_TT_NONE);
        }
    }
    else
    {
        if (sLBCommonExternSpriteAttr & SP_EXTERN)
        {
            sLBCommonExternSpriteAttr = ~sprite->attr;
        }
        if (sprite->attr & SP_EXTERN)
        {
            sLBCommonExternSpriteAttr = sprite->attr;
        }
        if (sprite->attr != sLBCommonExternSpriteAttr)
        {
            if (sprite->attr & SP_FASTCOPY)
            {
                if (!(sLBCommonExternSpriteAttr & SP_FASTCOPY))
                {
                    gDPSetCycleType(dl++, G_CYC_COPY);
                }
            }
            else if (sLBCommonExternSpriteAttr & SP_FASTCOPY)
            {
                gDPSetCycleType(dl++, G_CYC_1CYCLE);
            }
            if (sprite->attr & SP_TRANSPARENT)
            {
                if (!(sLBCommonExternSpriteAttr & SP_TRANSPARENT))
                {
                    gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
                }
            }
            else if (sprite->attr & SP_CLOUD)
            {
                if (!(sLBCommonExternSpriteAttr & SP_CLOUD))
                {
                    gDPSetRenderMode(dl++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
                }
            }
            else if (sLBCommonExternSpriteAttr & (SP_CLOUD | SP_TRANSPARENT))
            {
                gDPSetRenderMode(dl++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
            }
            if (sprite->attr & SP_TEXSHIFT)
            {
                if (!(sLBCommonExternSpriteAttr & SP_TEXSHIFT))
                {
                    gDPSetTextureFilter(dl++, G_TF_AVERAGE);
                }
            }
            else if (sLBCommonExternSpriteAttr & SP_TEXSHIFT)
            {
                gDPSetTextureFilter(dl++, G_TF_BILERP);
            }
        }
    }
    if (sprite->bmfmt != sLBCommonExternBitmapFmt)
    {
        switch (sprite->bmfmt)
        {
        case G_IM_FMT_I:
            gDPSetPrimColor(dl++, 0, 0, sprite->red, sprite->green, sprite->blue, sprite->alpha);
            gDPSetCombineLERP(dl++, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0);
            break;

        case G_IM_FMT_IA:
            gDPSetPrimColor(dl++, 0, 0, sprite->red, sprite->green, sprite->blue, sprite->alpha);
            gDPSetEnvColor(dl++, sobj->envcolor.r, sobj->envcolor.g, sobj->envcolor.b, sobj->envcolor.a);
            gDPSetCombineLERP(dl++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
            break;

        case G_IM_FMT_CI:
            gDPSetTextureLUT(dl++, G_TT_RGBA16);
            gDPLoadTLUT(dl++, sprite->nTLUT, sprite->startTLUT + 256, sprite->LUT);
            gDPLoadSync(dl++);

            /* fallthrough */

        default:
            gDPSetCombineMode(dl++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            break;
        }
        if (sprite->bmfmt != G_IM_FMT_CI)
        {
            if (sLBCommonExternBitmapFmt == G_IM_FMT_CI)
            {
                gDPSetTextureLUT(dl++, G_TT_NONE);
            }
        }
    }
    else switch (sprite->bmfmt)
    {
    case G_IM_FMT_I:
        gDPSetPrimColor(dl++, 0, 0, sprite->red, sprite->green, sprite->blue, sprite->alpha);
        break;

    case G_IM_FMT_IA:
        gDPSetPrimColor(dl++, 0, 0, sprite->red, sprite->green, sprite->blue, sprite->alpha);
        gDPSetEnvColor(dl++, sobj->envcolor.r, sobj->envcolor.g, sobj->envcolor.b, sobj->envcolor.a);
        break;

    case G_IM_FMT_CI:
        if (sprite->LUT != sLBCommonPrevSpriteLUT)
        {
            gDPLoadTLUT(dl++, sprite->nTLUT, sprite->startTLUT + 256, sprite->LUT);
            gDPLoadSync(dl++);
        }
        break;
    }
}

/* ---- lbcommon.c:2646-2807 lbCommonPrepSObjDraw 0x800CC818, verbatim,
 * REGION_US arms. The `// FAKE` temp_f24 assignment is dropped. ------ */
void lbCommonPrepSObjDraw(Gfx** dls, SObj* sobj) {
    Sprite* sprite;
    Bitmap* bitmap;
    s32 var_t1;
    f32 temp_f12;
    f32 temp_f14;
    s32 h;
    f32 temp_f24;
    f32 temp_f2;
    f32 var_f20;
    s32 temp_s4;
    s32 w;
    s32 i;
    s32 temp_s5;
    s32 yy;
    s32 temp_s1;
    s32 xx;
    s32 posx;
    s32 x;
    s32 posy;
    f32 sp90;
    s32 temp_t0;

    sprite = &sobj->sprite;
    if (sprite->scalex < 0.0001F)
    {
        return;
    }
    if (sprite->scaley < 0.0001F)
    {
        return;
    }
    bitmap = sprite->bitmap;
    if (bitmap == NULL)
    {
        return;
    }

    if (sobj->pos.x < 0.0F)
    {
        posx = sobj->pos.x - 0.9999F;
    }
    else
    {
        posx = sobj->pos.x;
    }

    if (sobj->pos.y < 0.0F)
    {
        posy = sobj->pos.y - 0.9999F;
    }
    else
    {
        posy = sobj->pos.y;
    }
    if ((posx >= sLBCommonScissorXMax) || (posy >= sLBCommonScissorYMax))
    {
        return;
    }

    if (sobj->cms == 2)
    {
        w = sprite->width;
    }
    else
    {
        w = sobj->lrs;
    }
    if (sobj->cmt == 2)
    {
        h = sprite->bmheight;
    }
    else
    {
        h = sobj->lrt;
    }
    if (sprite->attr & 0x20)
    {
        x = posy;
        xx = (sobj->pos.x + w);
        if (xx >= sLBCommonScissorXMin)
        {
            xx--;
            if (sprite->nbitmaps == 1)
            {
                lbCommonDrawSObjBitmap(dls, sobj, sprite, bitmap, posx, x, xx, x + h, 0, 0, 0x1000, 0x400);
                return;
            }

            for (i = 0; i < sprite->nbitmaps; i++)
            {
                yy = x + h;
                lbCommonDrawSObjBitmap(dls, sobj, sprite, bitmap, posx, x, xx, yy, 0, 0, 0x1000, 0x400);
                bitmap++;

                x = yy;
            }
        }
    }
    else
    {
        temp_s5 = sobj->pos.x + (w * sprite->scalex) + 0.9999F;
        temp_f2 = sprite->scalex;
        if (temp_s5 >= sLBCommonScissorXMin)
        {
            temp_f12 = sobj->pos.x - posx;
            temp_f14 = sobj->pos.y - posy;
            sp90 = sprite->scaley;
            if (sprite->nbitmaps == 1)
            {
                x = sobj->pos.y + (h * sp90) + 0.9999F;
                temp_s1 = (1024.0F / temp_f2) + 0.5F;
                xx = (1024.0F / sp90) + 0.5F;
                temp_s4 = (-(s32) (((temp_s1 * temp_f12) + 16.0F) / 32));
                temp_t0 = (-(s32) (((xx * temp_f14) + 16.0F) / 32));
                lbCommonDrawSObjBitmap(dls, sobj, sprite, bitmap, posx, posy, temp_s5, x, (s16)temp_s4, (s16)temp_t0, temp_s1, xx);
                return;
            }
            sp90 = sprite->scaley;
            temp_f24 = (f32) sprite->bmHreal * sp90;
            var_f20 = sobj->pos.y + h * sp90;
            x = (s32) var_f20;
            temp_s1 = (s32) ((1024.0F / temp_f2) + 0.5F);
            xx = (s32) ((1024.0F / sp90) + 0.5F);
            temp_s4 = (-(s32) (((temp_s1 * temp_f12) + 16.0F) / 32));
            temp_t0 = (-(s32) (((xx * temp_f14) + 16.0F) / 32));
            lbCommonDrawSObjBitmap(dls, sobj, sprite, bitmap, posx, posy, temp_s5, x, (s16)temp_s4, (s16)temp_t0, temp_s1, xx);
            bitmap++;

            for (i = 1; i < (sprite->nbitmaps - 1); i++)
            {
                var_t1 = x;
                temp_t0 = -((s32) ((s32) (xx * (var_f20 - x)) + 16) / 32);
                x = (s32) (var_f20 + temp_f24);
                lbCommonDrawSObjBitmap(dls, sobj, sprite, bitmap, posx, var_t1, temp_s5, x, (s16)temp_s4, (s16)temp_t0, temp_s1, xx);
                bitmap++;
                var_f20 += h * sp90;
            }
            var_t1 = x;
            temp_t0 = -((s32) ((s32) (xx * (var_f20 - x)) + 16) / 32);
            x = bitmap->actualHeight * sp90 + var_f20 + 0.9999F;
            lbCommonDrawSObjBitmap(dls, sobj, sprite, bitmap, posx, var_t1, temp_s5, x, (s16)temp_s4, (s16)temp_t0, temp_s1, xx);
        }
    }
}

/* ---- lbcommon.c:2810-2826, verbatim --------------------------------- */
// 0x800CCEAC
void lbCommonClearExternSpriteParams(void)
{
    sLBCommonExternSpriteAttr = SP_ARGUMENT;
    sLBCommonExternBitmapFmt = -1;

    sLBCommonPrevBitmapBuf = NULL;
    sLBCommonPrevSpriteLUT = NULL;
}

// 0x800CCED8
void lbCommonSetExternSpriteParams(Sprite *sprite)
{
    sLBCommonExternSpriteAttr = sprite->attr;
    sLBCommonExternBitmapFmt = sprite->bmfmt;
    sLBCommonPrevSpriteLUT = sprite->LUT;
}

/* ---- lbcommon.c:2828-2859, verbatim but for the display-list head,
 * which the port's functions ignore. ---------------------------------- */
// 0x800CCF00
void lbCommonDrawSObjAttr(GObj *gobj)
{
    SObj *sobj = SObjGetStruct(gobj);
    DBPERF_BEGIN(DBP_SPRITE);

    while (sobj != NULL)
    {
        if (!(sobj->sprite.attr & SP_HIDDEN))
        {
            lbCommonPrepSObjAttr(NULL, sobj);
            lbCommonPrepSObjDraw(NULL, sobj);
            lbCommonSetExternSpriteParams(&sobj->sprite);
        }
        sobj = sobj->next;
    }
    DBPERF_END(DBP_SPRITE);
}

// 0x800CCF74
void lbCommonDrawSObjNoAttr(GObj *gobj)
{
    SObj *sobj = SObjGetStruct(gobj);
    DBPERF_BEGIN(DBP_SPRITE);

    while (sobj != NULL)
    {
        if (!(sobj->sprite.attr & SP_HIDDEN))
        {
            lbCommonPrepSObjDraw(NULL, sobj);
            lbCommonClearExternSpriteParams();
        }
        sobj = sobj->next;
    }
    DBPERF_END(DBP_SPRITE);
}

/* ---- lbcommon.c:2862-2916 --------------------------------------------
 * DIVERGES: the G_IM_SIZ_4c decode is gone -- tools/export/ssb_spriteexport.py
 * unpacks those sprites at build time and the bank carries them as
 * G_IM_SIZ_4b -- and the decomp's missing `return sobj` is supplied. */
// 0x800CCFDC
SObj* lbCommonMakeSObjForGObj(GObj *gobj, Sprite *sprite)
{
    SObj *sobj;

    sobj = gcAddSObjForGObj(gobj, sprite);

    sobj->envcolor.r =
    sobj->envcolor.g =
    sobj->envcolor.b =
    sobj->envcolor.a = 0x00;

    sobj->maskt = sobj->masks = 0;

    sobj->cmt = sobj->cms = 2;

    sobj->pos.x = sobj->pos.y = 0.0F;

    return sobj;
}

// 0x800CD050
GObj* lbCommonMakeSpriteGObj
(
    u32 id,
    void (*func_run)(GObj*),
    s32 link,
    u32 link_priority,
    void (*proc_display)(GObj*),
    s32 dl_link,
    u32 dl_link_priority,
    u32 camera_tag,
    Sprite *sprite,
    u8 gobjproc_kind,
    void (*proc)(GObj*),
    u32 gobjproc_priority
)
{
    GObj *gobj = gcMakeGObjSPAfter(id, func_run, link, link_priority);

    if (gobj == NULL)
    {
        return NULL;
    }
    gcAddGObjDisplay(gobj, proc_display, dl_link, dl_link_priority, camera_tag);

    lbCommonMakeSObjForGObj(gobj, sprite);

    if (proc != NULL)
    {
        gcAddGObjProcess(gobj, proc, gobjproc_kind, gobjproc_priority);
    }
    return gobj;
}

/* lbcommon.c:2113-2120 lbCommonEjectGObjLinkedList 0x800D5C90, verbatim.
 * Ejects a whole link, tail first: it walks to the end of the link chain
 * and unwinds, so that gcEjectGObj never has to see the list it is
 * editing. The recursion is the game's -- a link is a handful of GObjs,
 * never a stack's worth. Note there is no NULL guard: the callers pass a
 * link they have just built (src/dc/ifcommon.c's pause menu), and an
 * empty one would be a bug the game would rather crash on than hide. */
// 0x800D5C90
void lbCommonEjectGObjLinkedList(GObj *gobj)
{
    if (gobj->link_next != NULL)
    {
        lbCommonEjectGObjLinkedList(gobj->link_next);
    }
    gcEjectGObj(gobj);
}

/* ---- lbcommon.c:2918-2967, verbatim through the RDP model. The port
 * also starts the pass's depth counter here. --------------------------- */
// 0x800CD0D0
void lbCommonStartSprite(Gfx **dls)
{
    Gfx *dl = NULL;

    (void)dls;
    (void)dl;

    lbCommonClearExternSpriteParams();

    gDPPipeSync(dl++);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gDPSetBlendColor(dl++, 0x00, 0x00, 0x00, 0x08);
    gDPSetAlphaCompare(dl++, G_AC_THRESHOLD);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureFilter(dl++, G_TF_BILERP);
    gDPSetTextureConvert(dl++, G_TC_FILT);
    gDPSetTextureDetail(dl++, G_TD_CLAMP);
    gDPSetTextureLOD(dl++, G_TL_TILE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetRenderMode(dl++, G_RM_OPA_SURF, G_RM_OPA_SURF2);

    lbCommonSpriteNextDepth();
    if (sLBCommonSpriteBackdrop)        /* looked, not taken */
    {
        sLBCommonSpriteBackdropCount--;
    }
    else
    {
        sLBCommonSpriteRectCount--;
    }
    sLBCommonRDP.dirty = TRUE;
}

/* ---- gDPFillRectangle under G_CC_PRIMITIVE ----------------------------
 * lb/lbfade.c:59-66 lbFadeProcDisplay and mn/mncommon/mnmessage.c:73-80
 * mnMessageTintProcDisplay: a flat colour over a screen rectangle,
 * blended by its alpha (G_RM_CLD_SURF, G_RM_AA_XLU_SURF), from a display
 * proc that runs as a camera of its own (objman.c func_80009F74 puts it
 * on the camera list) and so outside any sprite pass -- and
 * mn/mnvsmode/mnvsmode.c:912-920 mnVSModeRenderMenuName, the same
 * commands inside a sprite pass, under the sprites drawn after it. The
 * rectangle is in the game's pixels, lower-right exclusive as 1-cycle
 * mode has it; the quad carries its own header and takes the frame's
 * next depth, so it sits over what was drawn before it and under what
 * comes after, as the RDP's order had it. */
void lbCommonSpriteFillRect(s32 ulx, s32 uly, s32 lrx, s32 lry,
                            u32 r, u32 g, u32 b, u32 a)
{
    LBCommonSpriteQuad q;

#ifndef SSB_NO_DRAW
    /* Translucent pass only, whoever captured the caller. A sprite
     * camera already returns early outside that pass, but a display
     * proc on a 3D camera's DL link (func_80017EC0) runs in all three,
     * and a TR-list header sent while the opaque list is open spoils
     * the whole frame: the Sound Test and How to Play drew nothing at
     * all. */
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
#endif

    q.x0 = (f32)ulx * LB_SPRITE_SCREEN_SCALE;
    q.y0 = (f32)uly * LB_SPRITE_SCREEN_SCALE;
    q.x1 = (f32)lrx * LB_SPRITE_SCREEN_SCALE;
    q.y1 = (f32)lry * LB_SPRITE_SCREEN_SCALE;
    q.u0 = q.v0 = q.u1 = q.v1 = 0.0F;
    q.z = lbCommonSpriteNextDepth();
    q.argb = (a << 24) | (r << 16) | (g << 8) | b;
    q.oargb = 0;

#ifndef SSB_NO_DRAW
    {
        pvr_poly_cxt_t cxt;
        pvr_poly_hdr_t hdr;
        pvr_vertex_t v;
        int i;

        pvr_poly_cxt_col(&cxt, PVR_LIST_TR_POLY);
        cxt.gen.culling = PVR_CULLING_NONE;
        cxt.blend.src = PVR_BLEND_SRCALPHA;
        cxt.blend.dst = PVR_BLEND_INVSRCALPHA;
        cxt.depth.comparison = PVR_DEPTHCMP_GEQUAL;
        cxt.depth.write = PVR_DEPTHWRITE_DISABLE;
        DBPERF_COMPILE();
        pvr_poly_compile(&hdr, &cxt);
        pvr_prim(&hdr, sizeof(hdr));

        v.oargb = 0;
        v.argb = q.argb;
        v.z = q.z;
        v.u = v.v = 0.0F;
        for (i = 0; i < 4; i++)
        {
            v.flags = (i == 3) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v.x = (i & 2) ? q.x1 : q.x0;
            v.y = (i & 1) ? q.y0 : q.y1;
            pvr_prim(&v, sizeof(v));
        }
    }
    /* the next sprite pass rebinds: this header is not a sprite's */
    sLBCommonPrevBitmapBuf = NULL;
#else
    memset(&q.rect, 0, sizeof(q.rect));
    q.rect.rxh = ulx * 4;
    q.rect.ryh = uly * 4;
    q.rect.rxl = lrx * 4;
    q.rect.ryl = lry * 4;
    q.rect.copy = -1;                   /* a fill, not a sprite */
    if (gLBCommonSpriteQuadLogCount < LB_SPRITE_QUAD_LOG_MAX)
    {
        gLBCommonSpriteQuadLog[gLBCommonSpriteQuadLogCount] = q;
    }
    gLBCommonSpriteQuadLogCount++;
#endif
}

// 0x800CD1F0
void lbCommonSetSpriteScissor(s32 xmin, s32 xmax, s32 ymin, s32 ymax)
{
    sLBCommonScissorXMin = xmin;
    sLBCommonScissorYMin = ymin;
    sLBCommonScissorXMax = xmax;
    sLBCommonScissorYMax = ymax;
}

// 0x800CD214
void lbCommonFinishSprite(Gfx **dls)
{
    Gfx *dl = NULL;

    (void)dls;
    (void)dl;

    gDPSetCombineMode(dl++, G_CC_SHADE, G_CC_SHADE);
    gDPSetAlphaCompare(dl++, G_AC_NONE);
    gDPSetTexturePersp(dl++, G_TP_PERSP);

    if (sLBCommonExternSpriteAttr & SP_TRANSPARENT)
    {
        gDPSetRenderMode(dl++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    }
    if (sLBCommonExternBitmapFmt == G_IM_FMT_CI)
    {
        gDPSetTextureLUT(dl++, G_TT_NONE);
    }
}

/* ---- lbcommon.c:2969-3007 lbCommonDrawSprite 0x800CD2CC, verbatim.
 * A camera's proc_display: its viewport, clamped to the game's 10-pixel
 * border, is the sprite scissor, and every GObj on its DL links draws.
 *
 * The port adds two lines at the top: the frame runs every camera once
 * per PVR list (src/dc/taskman.c), and sprites belong to the translucent
 * one, so the other passes draw nothing -- but the opaque pass is where
 * the frame's border is decided, and this camera's viewport is part of
 * what the border must leave alone (objpvr.h gcClaimViewport). -------- */
void lbCommonDrawSprite(GObj *camera_gobj)
{
    CObj *cobj = CObjGetStruct(camera_gobj);
    Vp_t *viewport = &cobj->viewport.vp;
    s32 ulx = (viewport->vtrans[0] / 4) - (viewport->vscale[0] / 4);
    s32 uly = (viewport->vtrans[1] / 4) - (viewport->vscale[1] / 4);
    s32 lrx = (viewport->vtrans[0] / 4) + (viewport->vscale[0] / 4);
    s32 lry = (viewport->vtrans[1] / 4) + (viewport->vscale[1] / 4);

    if (gcGetDrawList() == PVR_LIST_OP_POLY)
    {
        /* the frame's border (taskman.c syTaskmanDrawBorder) blackens
         * what no camera claimed, and is painted on this pass */
        gcClaimViewport(viewport);
    }
    if (gcGetDrawList() != PVR_LIST_TR_POLY)
    {
        return;
    }
    if (ulx < (gSYVideoResWidth / GS_SCREEN_WIDTH_DEFAULT) * 10)
    {
        ulx = (gSYVideoResWidth / GS_SCREEN_WIDTH_DEFAULT) * 10;
    }
    if (uly < (gSYVideoResHeight / GS_SCREEN_HEIGHT_DEFAULT) * 10)
    {
        uly = (gSYVideoResHeight / GS_SCREEN_HEIGHT_DEFAULT) * 10;
    }
    if (lrx > gSYVideoResWidth - ((gSYVideoResWidth / GS_SCREEN_WIDTH_DEFAULT) * 10))
    {
        lrx = gSYVideoResWidth - ((gSYVideoResWidth / GS_SCREEN_WIDTH_DEFAULT) * 10);
    }
    if (lry > gSYVideoResHeight - ((gSYVideoResHeight / GS_SCREEN_HEIGHT_DEFAULT) * 10))
    {
        lry = gSYVideoResHeight - ((gSYVideoResHeight / GS_SCREEN_HEIGHT_DEFAULT) * 10);
    }
    lbCommonStartSprite(NULL);
    lbCommonSetSpriteScissor(ulx, lrx, uly, lry);

    gcCaptureCameraGObj(camera_gobj, (cobj->flags & COBJ_FLAG_IDENTIFIER) ? 1 : 0);
#ifdef DB_SPRITE_TRACE
    {
        static int n;

        if ((n++ % 120) == 0)
        {
            syDebugPrintf("sprtrace: cam %p prio %u mask %llx vp %d,%d-%d,%d rects %d\n",
                          (void *)camera_gobj, (unsigned)camera_gobj->dl_link_priority,
                          (unsigned long long)camera_gobj->camera_mask, ulx, uly, lrx, lry, (int)sLBCommonSpriteRectCount);
        }
    }
#endif
    lbCommonFinishSprite(NULL);
}

/* See lbcommon.h. The band is switched around the whole of
 * lbCommonDrawSprite so the scissor, the pass check and the capture are
 * the one implementation; every depth taken inside -- sprite rectangles,
 * fill quads, a model drawn layered -- comes from the backdrop band. */
void lbCommonDrawSpriteBackdrop(GObj *camera_gobj)
{
    sLBCommonSpriteBackdrop = TRUE;
    lbCommonDrawSprite(camera_gobj);
    sLBCommonSpriteBackdrop = FALSE;
}

/* lbcommon.c:2024-2064 0x800CB360 lbCommonDrawDObjScaleX and 2067-2072
 * 0x800CB4B0 lbCommonDObjScaleXProcDisplay.
 *
 * DIVERGES, and it collapses to one line. The decomp's walk is
 * gcDrawDObjTree's, character for character, with one difference: it
 * emits into gSYTaskmanDLHeads[1] rather than head 0, so the geometry
 * lands under the render mode ef/efdisplay.c's XLU display GObj set for
 * that head rather than the opaque one. The port has no display-list
 * heads -- the open PVR list is the camera pass's -- and which list a
 * batch belongs in is baked into the pack (FPackBatch.bucket) by the
 * exporter, which reads the head the game queued it into
 * (tools/export/ssb_effectexport.py ORBS_HEAD). So the head is already
 * accounted for and what is left is the ordinary model display proc.
 *
 * gGCScaleX, which the decomp saves and restores around each node, is
 * dead here for the same reason: it feeds the sprite matrix kinds
 * (objdisplay.c:786), and the port has none of those.
 *
 * ef/efmanager.c's damage orbs name this as their EFDesc's render proc,
 * which is why it exists. */
void lbCommonDObjScaleXProcDisplay(GObj *gobj)
{
    dc_model_proc_display(gobj);
}

/* lbcommon.c:3002-3008 0x800CD474 and 3020-3026 0x800CD538, verbatim: a
 * camera's projection and view from the object system's defaults, under
 * an XObj of the caller's kind. The magnify camera's two are the game's
 * own kinds, 0x4D and 0x4E (gm/gmcamera.c). */
void lbCommonInitCameraOrtho(CObj *cobj, u8 tk, u8 arg2)
{
    XObj *xobj = gcAddXObjForCamera(cobj, tk, arg2);

    cobj->projection.ortho = dGCOrthoDefault;
    cobj->projection.ortho.xobj = xobj;
}

void lbCommonInitCameraVec(CObj *cobj, u8 tk, u8 arg2)
{
    XObj *xobj = gcAddXObjForCamera(cobj, tk, arg2);

    cobj->vec = dGCCObjVecDefault;
    cobj->vec.xobj = xobj;
}

/* ---- the fighter tree's helpers ------------------------- */

/* lb/lbcommon.c:750-782 lbCommonGetTreeDObjNextFromRoot 0x800C86E8,
 * verbatim: depth first, children before siblings, up and across when a
 * limb runs out, NULL when the walk is back at `b`. */
DObj *lbCommonGetTreeDObjNextFromRoot(DObj *a, DObj *b)
{
    if (a->child != NULL)
    {
        a = a->child;
    }
    else if (a == b)
    {
        a = NULL;
    }
    else if (a->sib_next != NULL)
    {
        a = a->sib_next;
    }
    else while (TRUE)
    {
        if (a->parent == b)
        {
            a = NULL;

            break;
        }
        else if (a->parent->sib_next != NULL)
        {
            a = a->parent->sib_next;

            break;
        }
        else a = a->parent;
    }
    return a;
}

/* lb/lbcommon.c:807-832 lbCommonAddFighterPartsFigatree 0x800C87F4,
 * verbatim: one table entry per DObj of the walk from `root_dobj`, in the
 * walk's order -- which is why a hidden part linked above the hip takes
 * the leading slot. */
void lbCommonAddFighterPartsFigatree(DObj *root_dobj, void **figatree, f32 anim_frame)
{
    DObj *current_dobj = root_dobj;

    root_dobj->parent_gobj->anim_frame = anim_frame;

    while (current_dobj != NULL)
    {
        void *anim = *figatree;
        FTParts *parts = current_dobj->user_data.p;

        if (anim != NULL)
        {
            gcAddDObjAnimJoint(current_dobj, anim, anim_frame);

            parts->is_have_anim = TRUE;
        }
        else
        {
            current_dobj->anim_wait = AOBJ_ANIM_NULL;

            parts->is_have_anim = FALSE;
        }
        figatree++;

        current_dobj = lbCommonGetTreeDObjNextFromRoot(current_dobj, root_dobj);
    }
}

/* lb/lbcommon.c:893-906 lbCommonInitDObj 0x800C8A58, verbatim */
void lbCommonInitDObj(DObj *dobj, u8 tk1, u8 tk2, u8 tk3, u8 arg4)
{
    if (tk1 != nGCMatrixKindNull)
    {
        gcAddXObjForDObjFixed(dobj, tk1, arg4);
    }
    if (tk2 != nGCMatrixKindNull)
    {
        gcAddXObjForDObjFixed(dobj, tk2, arg4);
    }
    if (tk3 != nGCMatrixKindNull)
    {
        gcAddXObjForDObjFixed(dobj, tk3, arg4);
    }
    dobj->translate.vec = dGCTranslateDefault.vec;
    dobj->rotate.vec = dGCRotateDefaultRpy.vec;
    dobj->scale.vec = dGCScaleDefault.vec;
}

/* lb/lbcommon.c:784-805 lbCommonAddDObjAnimJointAll 0x800C8758, verbatim:
 * the AnimJoint counterpart of the figatree attach above,
 * one AObjEvent32 script per DObj of the tree walk from `root_dobj`. The
 * guard deals a shield-pose direction's table down the walk from XRotN
 * with it (ftcommon.c ftCommonGuardInitJoints). */
void lbCommonAddDObjAnimJointAll(DObj *root_dobj, AObjEvent32 **anim_joints, f32 anim_frame)
{
    DObj *current_dobj = root_dobj;

    root_dobj->parent_gobj->anim_frame = anim_frame;

    while (current_dobj != NULL)
    {
        AObjEvent32 *anim_joint = *anim_joints;

        if (anim_joint != NULL)
        {
            gcAddDObjAnimJoint(current_dobj, anim_joint, anim_frame);
        }
        else current_dobj->anim_wait = AOBJ_ANIM_NULL;

        anim_joints++;

        current_dobj = lbCommonGetTreeDObjNextFromRoot(current_dobj, root_dobj);
    }
}

/* lb/lbcommon.c:1112-1156 lbCommonAddTreeDObjsAnimAll 0x800C88AC,
 * verbatim: lbCommonAddDObjAnimJointAll's superset, one
 * anim_joints entry AND one matanim_joints table per DObj of the walk --
 * the matanim table itself is per-MObj of that DObj, walked inline. The
 * ground actors are the only port caller (ef/efground.c
 * efGroundCommonProcUpdate, efGroundSetupEffectDObjs); the fighter tree
 * still goes through lbCommonAddDObjAnimJointAll, which this does not
 * replace. */
void lbCommonAddTreeDObjsAnimAll(DObj *root_dobj, AObjEvent32 **anim_joints, AObjEvent32 ***p_matanim_joints, f32 anim_frame)
{
    DObj *current_dobj = root_dobj;

    root_dobj->parent_gobj->anim_frame = anim_frame;

    while (current_dobj != NULL)
    {
        if (anim_joints != NULL)
        {
            AObjEvent32 *anim_joint = *anim_joints;

            if (anim_joint != NULL)
            {
                gcAddDObjAnimJoint(current_dobj, anim_joint, anim_frame);
            }
            else current_dobj->anim_wait = AOBJ_ANIM_NULL;

            anim_joints++;
        }
        if (p_matanim_joints != NULL)
        {
            if (*p_matanim_joints != NULL)
            {
                MObj *mobj = current_dobj->mobj;
                AObjEvent32 **matanim_joints = *p_matanim_joints;

                while (mobj != NULL)
                {
                    AObjEvent32 *matanim_joint = *matanim_joints;

                    if (matanim_joint != NULL)
                    {
                        gcAddMObjMatAnimJoint(mobj, matanim_joint, anim_frame);
                    }
                    mobj = mobj->next;
                    matanim_joints++;
                }
            }
            p_matanim_joints++;
        }
        current_dobj = lbCommonGetTreeDObjNextFromRoot(current_dobj, root_dobj);
    }
}

/* lb/lbcommon.c:1169-1191 lbCommonAddMObjForTreeDObjs 0x800C9228,
 * verbatim: one MObjSub* table per DObj of the walk,
 * attached with gcAddMObjForDObj until the table's own NULL. */
void lbCommonAddMObjForTreeDObjs(DObj *root_dobj, MObjSub ***p_mobjsubs)
{
    DObj *current_dobj = root_dobj;

    while (current_dobj != NULL)
    {
        if (p_mobjsubs != NULL)
        {
            if (*p_mobjsubs != NULL)
            {
                MObjSub **mobjsubs = *p_mobjsubs;
                MObjSub *mobjsub = *mobjsubs;

                while (mobjsub != NULL)
                {
                    gcAddMObjForDObj(current_dobj, mobjsub);

                    mobjsubs++;
                    mobjsub = *mobjsubs;
                }
            }
            p_mobjsubs++;
        }
        current_dobj = lbCommonGetTreeDObjNextFromRoot(current_dobj, root_dobj);
    }
}

/* lb/lbcommon.c:1194-1203 lbCommonPlayTreeDObjsAnim 0x800C92B8, verbatim:
 * lbCommonAddTreeDObjsAnimAll's other half. The add walk
 * above only ATTACHES a script to each DObj of the tree; this one starts
 * every one of them, which is what gcPlayAnimAll does for a whole GObj
 * and what nothing does for a tree hanging off a DObj somebody else
 * owns. Board the Platforms' platforms are that case -- they hang off
 * the map's yakumono DObjs, whose GObj is the stage's. */
void lbCommonPlayTreeDObjsAnim(DObj *root_dobj)
{
    DObj *current_dobj = root_dobj;

    while (current_dobj != NULL)
    {
        gcPlayDObjAnimJoint(current_dobj);

        current_dobj = lbCommonGetTreeDObjNextFromRoot(current_dobj, root_dobj);
    }
}

/* lb/lbcommon.c:986-995 lbCommonInitDObj3Transforms 0x800C89BC, verbatim. */
void lbCommonInitDObj3Transforms(DObj *dobj, u8 tk1, u8 tk2, u8 tk3)
{
    gcAddDObj3TransformsKind(dobj, tk1, tk2, tk3);

    dobj->translate.vec = dGCTranslateDefault.vec;
    dobj->rotate.vec = dGCRotateDefaultRpy.vec;
    dobj->scale.vec = dGCScaleDefault.vec;
}

/* lb/lbcommon.c:1240-1257 lbCommonEjectTreeDObj 0x800C9424, verbatim:
 * unlinks dobj from its DObj tree, handing its own
 * children up to its parent (or, if dobj was the root, promoting the
 * first child to root by re-pointing the owning GObj's obj/obj_kind).
 * First caller is itManagerMakeItem, once an item's own DObjDesc data
 * exists to eject the throwaway root the custom-DObj setup builds. */
void lbCommonEjectTreeDObj(DObj *dobj)
{
    DObj *child_dobj = dobj->child;
    DObj *parent_dobj = dobj->parent;

    dobj->child = NULL;

    gcEjectDObj(dobj);

    if (parent_dobj == DOBJ_PARENT_NULL)
    {
        child_dobj->parent_gobj->obj = child_dobj;
        child_dobj->parent_gobj->obj_kind = nGCCommonAppendDObj;
    }
    else parent_dobj->child = child_dobj;

    child_dobj->parent = parent_dobj;
}

/* lb/lbcommon.c:1260-1344 lbCommonPlayTranslateScaledDObjAnim 0x800C9488,
 * verbatim: gcPlayDObjAnimJoint with the translation tracks scaled by
 * FTAttributes.translate_scales' vector for the joint -- the branch a
 * fighter with is_have_translate_scale takes (Luigi's attributes carry
 * a table, and the pack has carried it; ftcommon.c
 * ftParamUpdateAnimKeys). */
void lbCommonPlayTranslateScaledDObjAnim(DObj *dobj, Vec3f *scale)
{
    f32 interp;

    if (dobj->anim_wait != AOBJ_ANIM_NULL)
    {
        AObj *aobj = dobj->aobj;

        while (aobj != NULL)
        {
            if (aobj->kind != nGCAnimKindNone)
            {
                if (dobj->anim_wait != AOBJ_ANIM_END)
                {
                    aobj->length += dobj->anim_speed;
                }
                if (!(dobj->parent_gobj->flags & GOBJ_FLAG_NOANIM))
                {
                    switch (aobj->track)
                    {
                    case nGCAnimTrackRotX:
                        dobj->rotate.vec.f.x = gcGetAObjValue(aobj);
                        break;

                    case nGCAnimTrackRotY:
                        dobj->rotate.vec.f.y = gcGetAObjValue(aobj);
                        break;

                    case nGCAnimTrackRotZ:
                        dobj->rotate.vec.f.z = gcGetAObjValue(aobj);
                        break;

                    case nGCAnimTrackTraI:
                        interp = gcGetAObjValue(aobj);

                        if (interp < 0.0F)
                        {
                            interp = 0.0F;
                        }
                        else if (interp > 1.0F)
                        {
                            interp = 1.0F;
                        }
                        syInterpCubic(&dobj->translate.vec.f, aobj->interpolate, interp);

                        dobj->translate.vec.f.x *= scale->x;
                        dobj->translate.vec.f.y *= scale->y;
                        dobj->translate.vec.f.z *= scale->z;
                        break;

                    case nGCAnimTrackTraX:
                        dobj->translate.vec.f.x = gcGetAObjValue(aobj) * scale->x;
                        break;

                    case nGCAnimTrackTraY:
                        dobj->translate.vec.f.y = gcGetAObjValue(aobj) * scale->y;
                        break;

                    case nGCAnimTrackTraZ:
                        dobj->translate.vec.f.z = gcGetAObjValue(aobj) * scale->z;
                        break;

                    case nGCAnimTrackScaX:
                        dobj->scale.vec.f.x = gcGetAObjValue(aobj);
                        break;

                    case nGCAnimTrackScaY:
                        dobj->scale.vec.f.y = gcGetAObjValue(aobj);
                        break;

                    case nGCAnimTrackScaZ:
                        dobj->scale.vec.f.z = gcGetAObjValue(aobj);
                        break;
                    }
                }
            }
            aobj = aobj->next;
        }
        if (dobj->anim_wait == AOBJ_ANIM_END)
        {
            dobj->anim_wait = AOBJ_ANIM_NULL;
        }
    }
}

/* lb/lbcommon.c:955-1002 lbCommonAddMObjForFighterPartsDObj 0x800C8CB8,
 * verbatim: a fighter joint's MObjs, each with its costume
 * script played to the costume's frame and dropped, and a part's own
 * material script hung to play on. A costume's materials are baked
 * (src/dc/fighter.h FPackCostumes), so the port's callers hand it NULL
 * costume scripts; the MObjs they hand it are a part's animated ones out
 * of the pack (src/dc/ftparam.c ftParamGetPartMObjs), Samus's grapple
 * beam the only VS one, and NULL everywhere else. */
// 0x800C8CB8
void lbCommonAddMObjForFighterPartsDObj
(
    DObj *dobj,
    MObjSub **mobjsubs,
    AObjEvent32 **costume_matanim_joints,
    AObjEvent32 **main_matanim_joints,
    f32 anim_frame
)
{
    if (mobjsubs != NULL)
    {
        MObjSub *mobjsub = *mobjsubs;

        while (mobjsub != NULL)
        {
            MObj *mobj = gcAddMObjForDObj(dobj, mobjsub);

            if (costume_matanim_joints != NULL)
            {
                AObjEvent32 *costume_matanim_joint = *costume_matanim_joints;

                if (costume_matanim_joint != NULL)
                {
                    gcAddMObjMatAnimJoint(mobj, costume_matanim_joint, anim_frame);
                    gcParseMObjMatAnimJoint(mobj);
                    gcPlayMObjMatAnim(mobj);
                    gcRemoveAObjFromMObj(mobj);
                }
                costume_matanim_joints++;
            }
            if (main_matanim_joints != NULL)
            {
                AObjEvent32 *main_matanim_joint = *main_matanim_joints;

                if (main_matanim_joint != NULL)
                {
                    gcAddMObjMatAnimJoint(mobj, main_matanim_joint, 0.0F);
                    gcParseMObjMatAnimJoint(mobj);
                    gcPlayMObjMatAnim(mobj);
                }
                main_matanim_joints++;
            }
            mobjsubs++;
            mobjsub = *mobjsubs;
        }
    }
}
