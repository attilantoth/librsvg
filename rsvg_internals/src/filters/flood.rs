use cairo::{self, ImageSurface};
use cssparser;

use handle::RsvgHandle;
use node::{NodeResult, NodeTrait, RsvgCNodeImpl, RsvgNode};
use property_bag::PropertyBag;

use super::context::{FilterContext, FilterOutput, FilterResult};
use super::{Filter, FilterError, Primitive};

/// The `feFlood` filter primitive.
pub struct Flood {
    base: Primitive,
}

impl Flood {
    /// Constructs a new `Flood` with empty properties.
    #[inline]
    pub fn new() -> Flood {
        Flood {
            base: Primitive::new::<Self>(),
        }
    }
}

impl NodeTrait for Flood {
    #[inline]
    fn set_atts(
        &self,
        node: &RsvgNode,
        handle: *const RsvgHandle,
        pbag: &PropertyBag,
    ) -> NodeResult {
        self.base.set_atts(node, handle, pbag)
    }

    #[inline]
    fn get_c_impl(&self) -> *const RsvgCNodeImpl {
        self.base.get_c_impl()
    }
}

impl Filter for Flood {
    fn render(&self, node: &RsvgNode, ctx: &FilterContext) -> Result<FilterResult, FilterError> {
        let bounds = self.base.get_bounds(ctx).into_irect();

        let output_surface = ImageSurface::create(
            cairo::Format::ARgb32,
            ctx.source_graphic().get_width(),
            ctx.source_graphic().get_height(),
        ).map_err(FilterError::OutputSurfaceCreation)?;

        let cascaded = node.get_cascaded_values();
        let values = cascaded.get();

        let color = match values.flood_color.0 {
            cssparser::Color::CurrentColor => values.color.0,
            cssparser::Color::RGBA(rgba) => rgba,
        };
        let opacity = (values.flood_opacity.0).0;

        if opacity > 0f64 {
            let cr = cairo::Context::new(&output_surface);
            cr.rectangle(
                bounds.x0 as f64,
                bounds.y0 as f64,
                (bounds.x1 - bounds.x0) as f64,
                (bounds.y1 - bounds.y0) as f64,
            );
            cr.clip();

            cr.set_source_rgba(
                f64::from(color.red) / 255f64,
                f64::from(color.green) / 255f64,
                f64::from(color.blue) / 255f64,
                opacity,
            );
            cr.paint();
        }

        Ok(FilterResult {
            name: self.base.result.borrow().clone(),
            output: FilterOutput {
                surface: output_surface,
                bounds,
            },
        })
    }
}
